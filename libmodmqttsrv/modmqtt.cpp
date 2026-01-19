#include <string>
#include <regex>
#include <yaml-cpp/yaml.h>

#include "common.hpp"
#include "logging.hpp"
#include "dll_import.hpp"
#include "modmqtt.hpp"
#include "config.hpp"
#include "queue_item.hpp"
#include "mqttclient.hpp"
#include "modbus_messages.hpp"
#include "modbus_context.hpp"
#include "modbus_slave.hpp"
#include "conv_name_parser.hpp"
#include "yaml_converters.hpp"
#include "threadutils.hpp"
#include "strutils.hpp"


#include <csignal>
#include <iostream>

namespace
{
  volatile std::sig_atomic_t gSignalStatus = -1;
}

void signal_handler(int signal)
{
  gSignalStatus = signal;
  modmqttd::notifyQueues();
}

namespace modmqttd {

std::mutex gQueueMutex;
std::condition_variable gHasMessagesCondition;
bool gHasMessages = false;
std::shared_ptr<IModbusFactory> ModMqtt::mModbusFactory;


class RegisterConfigName {
    public:
        RegisterConfigName(const YAML::Node& data, const std::string& default_network, int default_slave) {
            std::string str = ConfigTools::readRequiredString(data, "register");
            StrUtils::trim(str);

            std::regex re("^([a-zA-Z][a-zA-Z0-9]+\\.)?([0-9]+\\.)?((0[xX])?[0-9a-fA-F]+)$");
            std::cmatch matches;

            if (!std::regex_match(str.c_str(), matches, re))
                throw ConfigurationException(data["register"].Mark(), "Invalid register specification");

            std::string network = matches[1];
            if (!network.empty()) {
                network.pop_back();
                mNetworkName = network;
            } else if (!default_network.empty()) {
                mNetworkName = default_network;
            } else {
                throw ConfigurationException(data["register"].Mark(), "Unknown network in register specification");
            }

            std::string slave = matches[2];
            if (!slave.empty()) {
                slave.pop_back();
                mSlaveId = std::stoi(slave);
            } else if (default_slave != -1) {
                mSlaveId = default_slave;
            } else {
                throw ConfigurationException(data["register"].Mark(), "Unknown slave id in register specification");
            }

            // for decimal use 1-based register number
            // for hex use 0-based
            mRegisterNumber = std::stoi(matches[3], nullptr, 0);
            std::string regNumMatch = matches[3].str();
            if(!(regNumMatch.size() > 1 && (regNumMatch[1] == 'x' || regNumMatch[1] == 'X'))) {
                if (mRegisterNumber == 0)
                    throw ConfigurationException(data["register"].Mark(), "Use hex address for 0-based register number");
                mRegisterNumber--;
            }
        };
        std::string mNetworkName;
        int mSlaveId = 0;
        int mRegisterNumber;
};

void
notifyQueues() {
    std::unique_lock<std::mutex> lock(gQueueMutex);
    gHasMessages = true;
	gHasMessagesCondition.notify_one();
}

RegisterType
parseRegisterType(const YAML::Node& data) {
    std::string rtype = "holding";
    ConfigTools::readOptionalValue<std::string>(rtype, data, "register_type");
    if (rtype == "coil")
        return RegisterType::COIL;
    if (rtype == "input")
        return RegisterType::INPUT;
    if (rtype == "holding")
        return RegisterType::HOLDING;
    if (rtype == "bit")
        return RegisterType::BIT;
    throw ConfigurationException(data.Mark(), std::string("Unknown register type ") + rtype);
}

PublishMode
parsePublishMode(const YAML::Node& data, PublishMode pDefault = PublishMode::ON_CHANGE) {
    std::string pmode;
    if (!ConfigTools::readOptionalValue<std::string>(pmode, data, "publish_mode"))
        return pDefault;

    if (pmode == "on_change") {
        return PublishMode::ON_CHANGE;
    } else if (pmode == "every_poll") {
        return PublishMode::EVERY_POLL;
    }

    throw ConfigurationException(data.Mark(), std::string("Invalid publish mode '") + pmode + "', valid values are: on_change, every_poll");
}

MqttObjectCommand::PayloadType
parsePayloadType(const YAML::Node& data) {
    //for future support for int and float mqtt command payload types
    std::string ptype = "string";
    ConfigTools::readOptionalValue<std::string>(ptype, data, "payload_type");
    if (ptype == "string")
        return MqttObjectCommand::PayloadType::STRING;
    throw ConfigurationException(data.Mark(), std::string("Unknown payload type ") + ptype);
}

ModMqtt::ModMqtt()
{
    // unit tests create main class multiple times
    // reset global flag at each creation
    gSignalStatus = -1;
    Mosquitto::libInit();
    mMqtt.reset(new MqttClient(*this));
    mModbusFactory.reset(new ModbusFactory());
}

void ModMqtt::init(const std::string& configPath) {
    ThreadUtils::set_thread_name("modmqtt");
    std::string targetPath(configPath);
    if (configPath.empty()) {
        spdlog::warn("No config path, trying to read config.yaml from working directory");
        targetPath = "./config.yaml";
    }
    YAML::Node config = YAML::LoadFile(targetPath);
    init(config);
}

void
ModMqtt::init(const YAML::Node& config) {
    initServer(config);
    initBroker(config);

    // must be before initObjects, we validate and use slave data
    // there
    ModbusInitData modbusData = initModbusClients(config);

    std::vector<MsgRegisterPollSpecification> mqtt_specs;
    std::vector<MqttObject> objects = initObjects(config, modbusData, mqtt_specs);


    for(std::vector<MsgRegisterPollSpecification>::iterator sit = modbusData.mPollSpecification.begin();
        sit != modbusData.mPollSpecification.end();
        sit++)
    {
        const std::string& netname = sit->mNetworkName;

        const auto& mqtt_spec = std::find_if(
            mqtt_specs.begin(), mqtt_specs.end(),
            [&netname](const auto& spec) -> bool { return spec.mNetworkName == netname; }
        );

        if (mqtt_spec == mqtt_specs.end()) {
            spdlog::error("No mqtt topics declared for [{}], ignoring poll group", netname);
            continue;
        } else {
            for(const auto& reg: mqtt_spec->mRegisters) {
                sit->merge(reg);
            }
        }

        std::vector<std::shared_ptr<ModbusClient>>::iterator client = std::find_if(
            mModbusClients.begin(), mModbusClients.end(),
            [&netname](const std::shared_ptr<ModbusClient>& client) -> bool { return client->mNetworkName == netname; }
        );
        if (client == mModbusClients.end()) {
            spdlog::error("Modbus client for network [{}] not initialized, ignoring specification", netname);
        } else {
            spdlog::debug("Sending register specification to modbus thread for network {}", netname);
            (*client)->mToModbusQueue.enqueue(QueueItem::create(*sit));
        }
    };

    //find which objects are related to final poll groups and create lists
    MqttClient::MqttPollObjMap mappedPollObjects;
    MqttClient::MqttCmdObjMap mappedCommandObjects;

    for(const MqttObject& obj : objects) {
        auto optr = std::shared_ptr<MqttObject>(new MqttObject(obj));
        for(std::vector<MsgRegisterPollSpecification>::const_iterator sit = modbusData.mPollSpecification.begin();
            sit != modbusData.mPollSpecification.end();
            sit++)
        {
            for(std::vector<MsgRegisterPoll>::const_iterator rit = sit->mRegisters.begin(); rit != sit->mRegisters.end(); rit++) {
                if (obj.hasRegisterIn(sit->mNetworkName, *rit)) {
                    MqttObjectRegisterIdent ident(sit->mNetworkName, *rit);
                    mappedPollObjects[ident].push_back(optr);
                }
            }
        }
        const std::map<std::string, MqttObjectCommand>& commands(mMqtt->getCommands());
        for(std::map<std::string, MqttObjectCommand>::const_iterator it = commands.begin(); it != commands.end(); it++) {
            if (obj.hasRegisterIn(it->second.mModbusNetworkName, it->second)) {
                assert(it->second.getCommandId() > 0);
                mappedCommandObjects[it->second.getCommandId()].push_back(optr);
            }
        }
    }

    mMqtt->setObjects(mappedPollObjects);
    mMqtt->setCommandObjects(mappedCommandObjects);
}

void
ModMqtt::initServer(const YAML::Node& config) {
    const YAML::Node& server = config["modmqttd"];
    if (!server.IsDefined())
        return;

    const YAML::Node& conv_paths = server["converter_search_path"];
    if (conv_paths.IsDefined()) {
        if (conv_paths.IsSequence()) {
            for(std::size_t i = 0; i < conv_paths.size(); i++) {
                mConverterPaths.push_back(ConfigTools::readRequiredValue<std::string>(conv_paths[i]));
            }
        } else if (conv_paths.IsScalar()) {
            mConverterPaths.push_back(ConfigTools::readRequiredValue<std::string>(conv_paths));
        }
    }

    const YAML::Node& conv_plugins = server["converter_plugins"];
    if (conv_plugins.IsDefined()) {
        if (!conv_plugins.IsSequence())
            throw ConfigurationException(conv_plugins.Mark(), "modmqttd.converter_plugins must be a list");

        for(std::size_t i = 0; i < conv_plugins.size(); i++) {
            const YAML::Node& path_node = conv_plugins[i];
            if (!path_node.IsScalar())
                throw ConfigurationException(path_node.Mark(), "Converter plugin path must be a string");

            std::string path = path_node.as<std::string>();

            try {
                std::shared_ptr<ConverterPlugin> plugin = initConverterPlugin(path);
                if (hasConverterPlugin(plugin->getName())) {
                    throw ConfigurationException(config.Mark(), std::string("Converter plugin ") + plugin->getName() + " already loaded");
                }

                spdlog::info("Added converter plugin {}", plugin->getName());
                mConverterPlugins.push_back(plugin);
            } catch (const std::exception& ex) {
                throw ConfigurationException(config.Mark(), ex.what());
            }
        }
    }
}

std::shared_ptr<ConverterPlugin>
ModMqtt::initConverterPlugin(const std::string& name) {
    std::string final_path;
    std::filesystem::path current_path = name;
    auto path_it = mConverterPaths.begin();
    do {
        spdlog::debug("Checking {}", current_path.c_str());
        if (std::filesystem::exists(current_path)) {
            final_path = current_path.string();
            break;
        }

        if (path_it == mConverterPaths.end()) {
            break;
        }

        current_path = *path_it;
        current_path.append(name);
        path_it++;
    } while(true);

    if (final_path.empty()) {
        throw ConvPluginNotFoundException(std::string("Converter plugin ") + name + " not found");
    }

    spdlog::debug("Trying to load converter plugin from ", final_path);

    std::shared_ptr<ConverterPlugin> plugin = modmqttd::dll_import<ConverterPlugin>(
        final_path,
        "converter_plugin"
    );

    return plugin;
}

void ModMqtt::initBroker(const YAML::Node& config) {
    const YAML::Node& mqtt = config["mqtt"];
    if (!mqtt.IsDefined())
        throw ConfigurationException(config.Mark(), "mqtt section is missing");

    std::string clientId = ConfigTools::readRequiredString(mqtt, "client_id");
    mMqtt->setClientId(clientId);

    const YAML::Node& broker = mqtt["broker"];
    if (!broker.IsDefined())
        throw ConfigurationException(config.Mark(), "no broker configuration in mqtt section");

    MqttBrokerConfig brokerConfig(broker);

    mMqtt->setBrokerConfig(brokerConfig);
    spdlog::debug("Broker configuration initialized");
}

std::vector<modmqttd::MsgRegisterPoll>
ModMqtt::readModbusPollGroups(const std::string& modbus_network, int default_slave, const YAML::Node& groups) {
    std::vector<modmqttd::MsgRegisterPoll> ret;

    if (!groups.IsDefined())
        return ret;

    if (!groups.IsSequence())
        throw ConfigurationException(groups.Mark(), "modbus network poll_groups must be a list");

    for(std::size_t i = 0; i < groups.size(); i++) {
        const YAML::Node& group(groups[i]);
        RegisterConfigName reg(group, modbus_network, default_slave);
        int count = ConfigTools::readRequiredValue<int>(group, "count");

        MsgRegisterPoll poll(reg.mSlaveId, reg.mRegisterNumber, parseRegisterType(group), count);
        // we do not set mRefreshMsec here, it should be merged
        // from mqtt overlapping groups
        // if no mqtt groups overlap, then modbus client will drop this poll group
        ret.push_back(poll);
    }

    return ret;
}


ModMqtt::ModbusInitData
ModMqtt::initModbusClients(const YAML::Node& config) {
    const YAML::Node& modbus = config["modbus"];
    if (!modbus.IsDefined())
        throw ConfigurationException(config.Mark(), "modbus section is missing");

    const YAML::Node& networks = modbus["networks"];
    if (!networks.IsDefined())
        throw ConfigurationException(modbus.Mark(), "modbus.networks section is missing");
    if (!networks.IsSequence())
        throw ConfigurationException(modbus.Mark(), "modbus.networks must be a list");

    if (networks.size() == 0)
        throw ConfigurationException(networks.Mark(), "No modbus networks defined");

    ModbusInitData ret;

    for(std::size_t i = 0; i < networks.size(); i++) {
        const YAML::Node& network(networks[i]);
        ModbusNetworkConfig modbus_config(network);

        //initialize modbus thread
        std::shared_ptr<ModbusClient> modbus(new ModbusClient());
        modbus->init(modbus_config);
        mModbusClients.push_back(modbus);

        MsgRegisterPollSpecification spec(modbus_config.mName);
        // send modbus slave configurations
        // for defined slaves
        const YAML::Node& slaves = network["slaves"];
        if (slaves.IsDefined()) {

            if (!slaves.IsSequence())
                throw ConfigurationException(slaves.Mark(), "slaves content must be a list");

            for(std::size_t i = 0; i < slaves.size(); i++) {
                const YAML::Node& ySlave(slaves[i]);
                std::vector<std::pair<int,int>> slave_addresses(ConfigTools::readRequiredValue<std::vector<std::pair<int,int>>>(ySlave, "address"));
                for(const std::pair<int,int>& addr_range: slave_addresses) {

                    if (addr_range.first < 0 || addr_range.second > 256 || addr_range.first > addr_range.second)
                        throw ConfigurationException(ySlave["address"].Mark(), "Invalid slave address range " + std::to_string(addr_range.first) + "-" + std::to_string(addr_range.second));

                    for(int addr = addr_range.first; addr <= addr_range.second; addr++) {
                        ModbusSlaveConfig slave_config(addr, ySlave);
                        modbus->mToModbusQueue.enqueue(QueueItem::create(slave_config));
                        spec.merge(readModbusPollGroups(modbus_config.mName, slave_config.mAddress, ySlave["poll_groups"]));

                        if (!slave_config.mSlaveName.empty())
                            ret.mSlaveNames[modbus->mNetworkName][slave_config.mAddress] = slave_config.mSlaveName;
                    }
                }
            }
        }

        const YAML::Node& old_groups(network["poll_groups"]);
        if (old_groups.IsDefined()) {
            spdlog::warn("'network.poll_groups' are deprecated and will be removed in future releases. Please use 'slaves' section and define per-slave poll_groups instead");
            spec.merge(readModbusPollGroups(modbus_config.mName, -1, old_groups));
        }
        ret.mPollSpecification.push_back(spec);
    }
    mMqtt->setModbusClients(mModbusClients);
    spdlog::debug("{} modbus client(s) initialized", mModbusClients.size());
    return ret;
}

MqttObject
ModMqtt::parseObject(
    const YAML::Node& pData,
    const std::string& pDefaultNetwork,
    int pDefaultSlaveId,
    const std::string& pSlaveName,
    std::chrono::milliseconds pDefaultRefresh,
    PublishMode pDefaultPublishMode,
    std::vector<MsgRegisterPollSpecification>& pSpecsOut)
{
    std::string topic(ConfigTools::readRequiredString(pData, "topic"));

    // fill network placeholder if any
    const std::string netPhVar("${network}");
    size_t netPhPos = topic.find(netPhVar);
    if (netPhPos != std::string::npos) {
        if (pDefaultNetwork.empty())
            throw ConfigurationException(pData["topic"].Mark(), "default network name must be set for topic " + topic);
        else
        {
            topic.replace(netPhPos, netPhVar.length(), pDefaultNetwork);
        }
    }

    //fill slave_address placeholder if any
    const std::string saPhVar("${slave_address}");
    size_t saPhPos = topic.find(saPhVar);
    if (saPhPos != std::string::npos) {
        if (pDefaultSlaveId == -1)
            throw ConfigurationException(pData["topic"].Mark(), "default slave address must be set for topic " + topic);
        else
        {
            topic.replace(saPhPos, saPhVar.length(), std::to_string(pDefaultSlaveId));
        }
    }

    //and the same for slave name
    const std::string snPhVar("${slave_name}");
    size_t snPhPos = topic.find(snPhVar);
    if (snPhPos != std::string::npos) {
        if (pDefaultSlaveId == -1) {
            throw ConfigurationException(pData["topic"].Mark(), "cannot find slave name, default slave address must be set for topic" + topic);
        } else if (pDefaultNetwork.empty()) {
            throw ConfigurationException(pData["topic"].Mark(), "default network name must be set for topic " + topic);
        } else if (pSlaveName.empty()) {
            throw ConfigurationException(pData["topic"].Mark(), std::string("missing name for slave id=") + std::to_string(pDefaultSlaveId) + " needed for placeholder in topic " + topic);
        } else {
            topic.replace(snPhPos, snPhVar.length(), pSlaveName);
        }
    }

    MqttObject ret(topic);
    spdlog::debug("processing object ", ret.getTopic());

    bool retain = true;
    if (ConfigTools::readOptionalValue<bool>(retain, pData, "retain"))
        ret.setRetain(retain);

    std::chrono::milliseconds everyPollRefresh = pDefaultRefresh;
    const YAML::Node& yState = pData["state"];

    PublishMode pmode = parsePublishMode(pData, pDefaultPublishMode);

    if (yState.IsDefined()) {
        if (yState.IsMap()) {
            MqttObjectDataNode node(parseObjectDataNode(yState, pDefaultNetwork, pDefaultSlaveId, pDefaultRefresh, pmode, everyPollRefresh, pSpecsOut));
            // a map that contains register with optional count
            // should output a list or a scalar value
            // in this case we do not need parsed parent level
            if (!node.hasConverter() && node.isUnnamed() && !node.isScalar()) {
                for (auto& n: node.getChildNodes())
                    ret.mState.addDataNode(n, true);
            } else {
                ret.mState.addDataNode(node);
            }
        } else if (yState.IsSequence()) {
            bool isUnnamed = false;
            for(size_t i = 0; i < yState.size(); i++) {
                const YAML::Node& yData = yState[i];
                MqttObjectDataNode node(parseObjectDataNode(yData, pDefaultNetwork, pDefaultSlaveId, pDefaultRefresh, pmode, everyPollRefresh, pSpecsOut));
                //the first element defines if we have named or unnamed list
                if (i == 0)
                    isUnnamed = node.isUnnamed();
                else {
                    if (node.isUnnamed() ^ isUnnamed)
                        throw ConfigurationException(yData.Mark(), "All list elements must be named or unnamed");
                }
                ret.mState.addDataNode(node, isUnnamed);
            }
        } else {
            throw ConfigurationException(yState.Mark(), "state must be a list or a single register data");
        }
    }

    const YAML::Node& yAvail = pData["availability"];

    ret.setPublishMode(pmode, everyPollRefresh);
    if (pmode == PublishMode::EVERY_POLL) {
        spdlog::debug("Min publish rate for {} set to {}ms", ret.getStateTopic(), std::chrono::duration_cast<std::chrono::milliseconds>(everyPollRefresh).count());
    }

    if (!yAvail.IsDefined())
        return ret;

    std::string availValue;
    if (ConfigTools::readOptionalValue<std::string>(availValue, yAvail, "available_value"))
        ret.setAvailableValue(MqttValue::fromString(availValue));

    if (yAvail.IsMap()) {
        std::chrono::milliseconds unused = std::chrono::milliseconds::zero();
        MqttObjectDataNode node(parseObjectDataNode(yAvail, pDefaultNetwork, pDefaultSlaveId, pDefaultRefresh, ret.getPublishMode(), unused, pSpecsOut));
        if (!node.isScalar() && !node.hasConverter())
            throw ConfigurationException(yAvail.Mark(), "multiple registers availability must use a converter");
        ret.addAvailabilityDataNode(node);
    } else {
        throw ConfigurationException(yState.Mark(), "availability must be a single register or a list with converter");
    }

    return ret;
};

MqttObjectDataNode
ModMqtt::parseObjectDataNode(
    const YAML::Node& pNode,
    const std::string& pDefaultNetwork,
    int pDefaultSlaveId,
    std::chrono::milliseconds pRefresh,
    PublishMode pMode,
    std::chrono::milliseconds& pEveryPollRefreshOut,
    std::vector<MsgRegisterPollSpecification>& pSpecsOut
    )
{

    MqttObjectDataNode node;

    std::string name;
    if (ConfigTools::readOptionalValue<std::string>(name, pNode, "name"))
        node.setName(name);

    if (ConfigTools::readOptionalValue<std::chrono::milliseconds>(pRefresh, pNode, "refresh")) {
        if (pRefresh < pEveryPollRefreshOut)
            pEveryPollRefreshOut = pRefresh;
    }

    const YAML::Node& converter = pNode["converter"];
    if (converter.IsDefined()) {
        node.setConverter(createConverter(converter));
    }

    const YAML::Node& yRegisters = pNode["registers"];
    if (yRegisters.IsDefined()) {
        bool isUnnamed = false;
        if (!yRegisters.IsSequence())
            throw ConfigurationException(yRegisters.Mark(), "'registers' must be a list");
        for(size_t i = 0; i < yRegisters.size(); i++) {
            const YAML::Node& yData = yRegisters[i];
            MqttObjectDataNode childNode(parseObjectDataNode(yData, pDefaultNetwork, pDefaultSlaveId, pRefresh, pMode, pEveryPollRefreshOut, pSpecsOut));
            //the first element defines if we have named or unnamed list
            if (i == 0)
                isUnnamed = node.isUnnamed();
            else {
                if (node.isUnnamed() ^ isUnnamed)
                    throw ConfigurationException(yData.Mark(), "All list elements must be named or unnamed");
            }
            node.addChildDataNode(childNode, true);
        }
    } else {
        int count = 1;
        ConfigTools::readOptionalValue<int>(count, pNode, "count");

        MqttObjectRegisterIdent first_ident = updateSpecification(pNode, count, pRefresh, pDefaultNetwork, pDefaultSlaveId, pMode, pSpecsOut);
        if (count == 1) {
            node.setScalarNode(first_ident);
        } else {
            int end_idx = first_ident.mRegisterNumber + count;
            for(int i = first_ident.mRegisterNumber; i < end_idx; i++) {
                first_ident.mRegisterNumber = i;
                MqttObjectDataNode childNode;
                childNode.setScalarNode(first_ident);
                node.addChildDataNode(childNode);
            }
        }
    }

    return node;
}


MqttObjectCommand
ModMqtt::parseObjectCommand(
    const std::string& pTopicPrefix,
    int nextCommandId,
    const YAML::Node& node,
    const std::string& default_network,
    int default_slave)
{
    std::string name = ConfigTools::readRequiredString(node, "name");
    std::string topic = pTopicPrefix + "/" + name;
    RegisterConfigName rname(node, default_network, default_slave);
    RegisterType rType = parseRegisterType(node);
    MqttObjectCommand::PayloadType pType = parsePayloadType(node);
    int count = 1;
    ConfigTools::readOptionalValue<int>(count, node, "count");

    MqttObjectCommand cmd(
        nextCommandId,
        topic,
        pType,
        rname.mNetworkName,
        rname.mSlaveId,
        rType,
        rname.mRegisterNumber,
        count
    );

    const YAML::Node& converter = node["converter"];
    if (converter.IsDefined()) {
        cmd.setConverter(createConverter(converter));
    }

    return cmd;
}


std::shared_ptr<DataConverter>
ModMqtt::createConverter(const YAML::Node& node) const {
    if (!node.IsScalar())
        throw ConfigurationException(node.Mark(), "converter must be a string");
    std::string line = ConfigTools::readRequiredValue<std::string>(node);

    try {
        ConverterSpecification spec(ConverterNameParser::parse(line));

        std::shared_ptr<DataConverter> conv = createConverterInstance(spec.plugin, spec.converter);
        if (conv == nullptr)
            throw ConfigurationException(node.Mark(), "Converter " + spec.plugin + "." + spec.converter + " not found");
        try {
            conv->setArgs(spec.args);
        } catch (const std::exception& ex) {
            throw ConfigurationException(node.Mark(), ex.what());
        }
        return conv;
    } catch (const ConvNameParserException& ex) {
        throw ConfigurationException(node.Mark(), ex.what());
    }
}


std::shared_ptr<DataConverter>
ModMqtt::createConverterInstance(const std::string pluginName, const std::string& converterName) const {
    auto it = std::find_if(
        mConverterPlugins.begin(),
        mConverterPlugins.end(),
        [&pluginName](const std::shared_ptr<ConverterPlugin>& plugin)
            -> bool { return plugin->getName() == pluginName; }

    );
    if (it == mConverterPlugins.end()) {
        return nullptr;
    }

    std::shared_ptr<DataConverter> ret((*it)->getConverter(converterName));

    return ret;
}

int
ModMqtt::parseObjectCommands(
    const std::string& pTopicPrefix,
    int nextCommandId,
    const YAML::Node& commands,
    const std::string& default_network,
    int default_slave
) {
    if (commands.IsDefined()) {
        if (commands.IsMap()) {
            mMqtt->addCommand(parseObjectCommand(pTopicPrefix, nextCommandId++, commands, default_network, default_slave));
        } else if (commands.IsSequence()) {
            for(size_t i = 0; i < commands.size(); i++) {
                const YAML::Node& cmddata = commands[i];
                mMqtt->addCommand(parseObjectCommand(pTopicPrefix, nextCommandId++, cmddata, default_network, default_slave));
            }
        }
    }
    return nextCommandId;
}

std::vector<MqttObject>
ModMqtt::initObjects(const YAML::Node& config, const ModMqtt::ModbusInitData& modbusData, std::vector<MsgRegisterPollSpecification>& pSpecsOut)
{
    std::vector<MqttObjectCommand> commands;
    std::vector<MqttObject> objects;
    int nextCommandId = 1;


    const YAML::Node& mqtt = config["mqtt"];
    if (!mqtt.IsDefined())
        throw ConfigurationException(config.Mark(), "mqtt section is missing");

    auto defaultRefresh = std::chrono::milliseconds(5000);
    ConfigTools::readOptionalValue<std::chrono::milliseconds>(defaultRefresh, mqtt, "refresh");

    PublishMode defaultPublishMode = parsePublishMode(mqtt);

    const YAML::Node& config_objects = mqtt["objects"];
    if (!config_objects.IsDefined())
        throw ConfigurationException(mqtt.Mark(), "objects section is missing");

    if (!config_objects.IsSequence())
        throw ConfigurationException(mqtt.Mark(), "mqtt.objects must be a list");

    for(std::size_t i = 0; i < config_objects.size(); i++) {
        const YAML::Node& objdata = config_objects[i];

        auto objectRefresh = defaultRefresh;
        ConfigTools::readOptionalValue<std::chrono::milliseconds>(objectRefresh, objdata, "refresh");

        std::vector<std::string> networks;
        ConfigTools::readOptionalValue<std::vector<std::string>>(networks, objdata, "network");

        // default undefined network - register address must contain network name
        if (networks.size() == 0)
            networks.push_back(std::string());

        std::vector<std::pair<int, int>> slaves;
        ConfigTools::readOptionalValue<std::vector<std::pair<int, int>>>(slaves, objdata, "slave");
        // default undefined slave - register address must contain slave id
        if (slaves.size() == 0)
            slaves.push_back(std::pair<int,int>(-1, -1));

        for(const std::string& currentNetwork: networks) {
            if (currentNetwork.size() != 0) {
                std::vector<std::shared_ptr<ModbusClient>>::const_iterator cit = std::find_if(
                    mModbusClients.begin(), mModbusClients.end(),
                    [&currentNetwork](const std::shared_ptr<ModbusClient>& cptr) -> bool { return currentNetwork == cptr->mNetworkName; }
                    );
                if (cit == mModbusClients.end())
                    throw ConfigurationException(objdata["network"].Mark(), std::string("Unknown modbus network ") + currentNetwork);
            }

            std::set<int> created;
            for (const std::pair<int, int>& slave_range: slaves) {
                if (slave_range.first != -1 && (slave_range.first < 0 || slave_range.second > 256 || slave_range.first > slave_range.second))
                    throw ConfigurationException(objdata["slave"].Mark(), std::string("Invalid slave range ") + std::to_string(slave_range.first) + "-" + std::to_string(slave_range.second));

                for (int defaultSlaveId = slave_range.first; defaultSlaveId <= slave_range.second; defaultSlaveId++) {
                    if (created.find(defaultSlaveId) != created.end())
                        throw ConfigurationException(objdata["slave"].Mark(), std::string("Slave with id=") + std::to_string(defaultSlaveId) + " is duplicated in the list");

                    MqttObject object(parseObject(
                        objdata,
                        currentNetwork,
                        defaultSlaveId,
                        modbusData.getSlaveName(currentNetwork, defaultSlaveId),
                        objectRefresh,
                        defaultPublishMode,
                        pSpecsOut)
                    );
                    const std::string& baseTopic(object.getTopic());

                    std::vector<MqttObject>::const_iterator oit = std::find_if(
                        objects.begin(),  objects.end(),
                        [&baseTopic](const MqttObject& old) -> bool { return old.getTopic() == baseTopic; }
                        );
                    if (oit != objects.end())
                        throw ConfigurationException(objdata.Mark(), std::string("Topic ") + baseTopic + " already defined. Missing network or slave placeholder?");


                    objects.push_back(object);
                    nextCommandId = parseObjectCommands(object.getTopic(), nextCommandId, objdata["commands"], currentNetwork, defaultSlaveId);
                    spdlog::debug("object for topic {} created", object.getTopic());
                    created.insert(defaultSlaveId);
                }
            }
        }
    }
    spdlog::debug("Finished reading mqtt object declarations");
    return objects;
}

MqttObjectRegisterIdent
ModMqtt::updateSpecification(
    const YAML::Node& data,
    int pRegisterCount,
    const std::chrono::milliseconds& pCurrentRefresh,
    const std::string& pDefaultNetwork,
    int pDefaultSlaveId,
    PublishMode pCurrentMode,
    std::vector<MsgRegisterPollSpecification>& specs)
{
    const RegisterConfigName rname(data, pDefaultNetwork, pDefaultSlaveId);

    MsgRegisterPoll poll(rname.mSlaveId, rname.mRegisterNumber, parseRegisterType(data), pRegisterCount);
    poll.mRefreshMsec = pCurrentRefresh;
    poll.mPublishMode = pCurrentMode;

    // find network poll specification or create one
    std::vector<MsgRegisterPollSpecification>::iterator spec_it = std::find_if(
        specs.begin(), specs.end(),
        [&rname](const MsgRegisterPollSpecification& s) -> bool { return s.mNetworkName == rname.mNetworkName; }
        );

    if (spec_it == specs.end()) {
        specs.insert(specs.begin(), MsgRegisterPollSpecification(rname.mNetworkName));
        spec_it = specs.begin();
    }

    spec_it->merge(poll);

    return MqttObjectRegisterIdent(rname.mNetworkName, rname.mSlaveId, poll.mRegisterType, poll.mRegister);
}

void ModMqtt::start() {
    std::signal(SIGTERM, signal_handler);

    // mosquitto does not use reconnect_delay_set
    // when doing inital connection. We also do not want to
    // process queues before connection to mqtt broker is
    // established - this will cause availability messages to be dropped.

    // TODO if broker is down and modbus is up then mSlaveQueues will grow forever and
    // memory allocated by queues will never be released. Add MsgStartPolling?
    spdlog::debug("Performing initial connection to mqtt broker");
    do {
        mMqtt->start();
        if (mMqtt->isConnected()) {
            spdlog::debug("Broker connected, entering main loop");
            break;
        }
        waitForSignal();
    } while(gSignalStatus == -1);

    while(mMqtt->isStarted()) {
        if (gSignalStatus == -1) {
            waitForQueues();
            processModbusMessages();
        } else if (gSignalStatus > 0) {
            int currentSignal = gSignalStatus;
            gSignalStatus = -1;
            if (currentSignal == SIGTERM) {
                spdlog::info("Got SIGTERM, exiting…");
                break;
            } else if (currentSignal == SIGHUP) {
                //TODO reload configuration, reconnect broker and
                //create new list of modbus clients if needed
            }
            currentSignal = -1;
        } else if (gSignalStatus == 0) {
            spdlog::info("Got stop request, exiting…");
            break;
        }
    };

    spdlog::info("Stopping modbus clients");
    for(std::vector<std::shared_ptr<ModbusClient>>::iterator client = mModbusClients.begin();
        client < mModbusClients.end(); client++)
    {
        (*client)->stop();
    }

    //process mqtt queue after modbus clients are stopped
    processModbusMessages();

    if (mMqtt->isConnected()) {
        spdlog::info("Publishing availability status 0 for all registers");
        for(std::vector<std::shared_ptr<ModbusClient>>::iterator client = mModbusClients.begin();
            client < mModbusClients.end(); client++)
        {
            mMqtt->processModbusNetworkState((*client)->mNetworkName, false);
        }
    }

    spdlog::debug("Shutting down mosquitto client");
    // If connected, then shutdown()
    // will send disconnection request to mqtt broker.
    // After disconnection mMqtt will notify global queue mutex
    // Otherwise we are already stopped.
    mMqtt->shutdown();
    if (mMqtt->isStarted()) {
        spdlog::debug("Waiting for disconnection event");
        waitForQueues();
    }

    //TODO mosquitto thread could add some messages to
    //mModbusClients queue between ModbusClient::stop() and MqttClient::shutdown()
    //Cleanup those messages here
    spdlog::info("Shutdown finished");
}

void
ModMqtt::stop() {
    spdlog::debug("Sending stop request to ModMqtt server");
    gSignalStatus = 0;
    notifyQueues();
}

void
ModMqtt::processModbusMessages() {
    QueueItem item;
    for(std::vector<std::shared_ptr<ModbusClient>>::iterator client = mModbusClients.begin();
        client < mModbusClients.end(); client++)
    {
        while ((*client)->mFromModbusQueue.try_dequeue(item)) {
            if (item.isSameAs(typeid(MsgRegisterValues))) {
                std::unique_ptr<MsgRegisterValues> val(item.getData<MsgRegisterValues>());
                mMqtt->processRegisterValues((*client)->mNetworkName, *val);
            } else if (item.isSameAs(typeid(MsgRegisterReadFailed))) {
                std::unique_ptr<MsgRegisterReadFailed> val(item.getData<MsgRegisterReadFailed>());
                mMqtt->processRegistersOperationFailed((*client)->mNetworkName, *val);
            } else if (item.isSameAs(typeid(MsgRegisterWriteFailed))) {
                std::unique_ptr<MsgRegisterWriteFailed> val(item.getData<MsgRegisterWriteFailed>());
                mMqtt->processRegistersOperationFailed((*client)->mNetworkName, *val);
            } else if (item.isSameAs(typeid(MsgModbusNetworkState))) {
                std::unique_ptr<MsgModbusNetworkState> val(item.getData<MsgModbusNetworkState>());
                mMqtt->processModbusNetworkState(val->mNetworkName, val->mIsUp);
            } else {
                spdlog::error("Unknown message from modbus thread, ignoring");
            }
        }
    }
}

bool
ModMqtt::hasConverterPlugin(const std::string& name) const {
    auto it = std::find_if(
        mConverterPlugins.begin(),
        mConverterPlugins.end(),
        [&name](const std::shared_ptr<ConverterPlugin>& plugin)
            -> bool { return plugin->getName() == name; }

    );
    return it != mConverterPlugins.end();
}



void
ModMqtt::waitForSignal() {
    std::unique_lock<std::mutex> lock(gQueueMutex);
    gHasMessagesCondition.wait_for(lock, std::chrono::seconds(5));
}

void
ModMqtt::waitForQueues() {
    std::unique_lock<std::mutex> lock(gQueueMutex);
    gHasMessagesCondition.wait(lock, []{ return gHasMessages;});
    gHasMessages = false;
}

void
ModMqtt::setMqttImplementation(const std::shared_ptr<IMqttImpl>& impl) {
    mMqtt->setMqttImplementation(impl);
}

void
ModMqtt::setModbusContextFactory(const std::shared_ptr<IModbusFactory>& factory) {
    mModbusFactory = factory;
}

ModMqtt::~ModMqtt() {
    Mosquitto::libCleanup();
    // we need to delete all conveter instances
    // from plugins before unloading plugin libraries
    mMqtt = nullptr;
}

} //namespace
