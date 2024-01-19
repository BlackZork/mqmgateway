#include <string>
#include <regex>
#include <yaml-cpp/yaml.h>
#include <boost/dll/import.hpp>
#include <boost/algorithm/string.hpp>


#include "common.hpp"
#include "modmqtt.hpp"
#include "config.hpp"
#include "queue_item.hpp"
#include "mqttclient.hpp"
#include "modbus_messages.hpp"
#include "modbus_context.hpp"
#include "modbus_slave.hpp"
#include "conv_name_parser.hpp"
#include "yaml_converters.hpp"

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
            boost::trim(str);

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
            if(!(regNumMatch.size() > 1 && (regNumMatch[1] == 'x' || regNumMatch[1] == 'X')))
                mRegisterNumber--;
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
    std::string targetPath(configPath);
    if (configPath.empty()) {
        BOOST_LOG_SEV(log, Log::warn) << "No config path, trying to read config.yaml from working directory";
        targetPath = "./config.yaml";
    }
    YAML::Node config = YAML::LoadFile(targetPath);
    init(config);
}

void
ModMqtt::init(const YAML::Node& config) {
    initServer(config);
    initBroker(config);
    std::vector<MsgRegisterPollSpecification> mqtt_specs = initObjects(config);

    std::vector<MsgRegisterPollSpecification> modbus_specs = initModbusClients(config);

    for(std::vector<MsgRegisterPollSpecification>::iterator sit = modbus_specs.begin(); sit != modbus_specs.end(); sit++) {
        const std::string& netname = sit->mNetworkName;

        const auto& mqtt_spec = std::find_if(
            mqtt_specs.begin(), mqtt_specs.end(),
            [&netname](const auto& spec) -> bool { return spec.mNetworkName == netname; }
        );

        if (mqtt_spec == mqtt_specs.end()) {
            BOOST_LOG_SEV(log, Log::error) << "No mqtt topics declared for [" << netname << "], ignoring poll group";
            continue;
        } else {
            for(const auto& reg: mqtt_spec->mRegisters) {
                sit->merge(reg);
            }
        }

        std::vector<std::shared_ptr<ModbusClient>>::iterator client = std::find_if(
            mModbusClients.begin(), mModbusClients.end(),
            [&netname](const std::shared_ptr<ModbusClient>& client) -> bool { return client->mName == netname; }
        );
        if (client == mModbusClients.end()) {
            BOOST_LOG_SEV(log, Log::error) << "Modbus client for network [" << netname << "] not initialized, ignoring specification";
        } else {
            BOOST_LOG_SEV(log, Log::debug) << "Sending register specification to modbus thread for network " << netname;
            (*client)->mToModbusQueue.enqueue(QueueItem::create(*sit));
        }
    };
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
                boost::shared_ptr<ConverterPlugin> plugin = initConverterPlugin(path);
                if (hasConverterPlugin(plugin->getName())) {
                    throw ConfigurationException(config.Mark(), std::string("Converter plugin ") + plugin->getName() + " already loaded");
                }

                BOOST_LOG_SEV(log, Log::info) << "Added converter plugin " << plugin->getName();
                mConverterPlugins.push_back(plugin);
            } catch (const std::exception& ex) {
                throw ConfigurationException(config.Mark(), ex.what());
            }
        }
    }
}

boost::shared_ptr<ConverterPlugin>
ModMqtt::initConverterPlugin(const std::string& name) {
    std::string final_path;
    boost::filesystem::path current_path = name;
    auto path_it = mConverterPaths.begin();
    do {
        BOOST_LOG_SEV(log, Log::debug) << "Checking " << current_path;
        if (boost::filesystem::exists(current_path)) {
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

    BOOST_LOG_SEV(log, Log::debug) << "Trying to load converter plugin from " << final_path;

    boost::shared_ptr<ConverterPlugin> plugin = boost_dll_import<ConverterPlugin>(
        final_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
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
    BOOST_LOG_SEV(log, Log::debug) << "Broker configuration initialized";
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

        MsgRegisterPoll poll(reg.mRegisterNumber, count);
        poll.mSlaveId = reg.mSlaveId;
        poll.mRegisterType = parseRegisterType(group);
        poll.mCount = count;
        // we do not set mRefreshMsec here, it should be merged
        // from mqtt overlapping groups
        // if no mqtt groups overlap, then modbus client will drop this poll group
        ret.push_back(poll);
    }

    return ret;
}


std::vector<MsgRegisterPollSpecification>
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

    std::vector<MsgRegisterPollSpecification> ret;

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
                throw new ConfigurationException(slaves.Mark(), "slaves content should be a list");

            for(std::size_t i = 0; i < slaves.size(); i++) {
                const YAML::Node& slave(slaves[i]);
                ModbusSlaveConfig slave_config(slave);
                modbus->mToModbusQueue.enqueue(QueueItem::create(slave_config));

                spec.merge(readModbusPollGroups(modbus_config.mName, slave_config.mAddress, slave["poll_groups"]));
            }
        }

        const YAML::Node& old_groups(network["poll_groups"]);
        if (old_groups.IsDefined()) {
            BOOST_LOG_SEV(log, Log::warn) << "'network.poll_groups' are deprecated and will be removed in future releases. Please use 'slaves' section and define per-slave poll_groups instead";
            spec.merge(readModbusPollGroups(modbus_config.mName, -1, old_groups));
        }
        ret.push_back(spec);
    }
    mMqtt->setModbusClients(mModbusClients);
    BOOST_LOG_SEV(log, Log::debug) << mModbusClients.size() << " modbus client(s) initialized";
    return ret;
}

bool
ModMqtt::parseAndAddRefresh(std::stack<std::chrono::milliseconds>& values, const YAML::Node& parent) {
    std::chrono::milliseconds ts;
    if (!ConfigTools::readOptionalValue<std::chrono::milliseconds>(ts, parent, "refresh"))
        return false;

    values.push(ts);
    return true;
}

void
ModMqtt::readObjectState(
    MqttObject& object,
    const std::string& default_network,
    int default_slave,
    std::vector<MsgRegisterPollSpecification>& specs_out,
    std::stack<std::chrono::milliseconds>& currentRefresh,
    const YAML::Node& state)
{
    if (!state.IsDefined())
        return;

    bool is_unnamed = true;
    std::vector<MsgRegisterPollSpecification> specs_in;

    if (state.IsMap()) {
        //a map can contain name, converter and one or more registers
        std::string name;
        if (ConfigTools::readOptionalValue<std::string>(name, state, "name"))
            is_unnamed = false;
        const YAML::Node& converter = state["converter"];
        if (converter.IsDefined()) {
            object.mState.setConverter(createConverter(converter));
        }
        const YAML::Node& node = state["registers"];
        if (node.IsDefined()) {
            if (!node.IsSequence())
                throw ConfigurationException(node.Mark(), "registers content should be a list");
            for(size_t i = 0; i < node.size(); i++) {
                const YAML::Node& regdata = node[i];
                readObjectStateNode(object, default_network, default_slave, specs_in, currentRefresh, name, regdata, true);
            };
        } else {
            //single named register + optional count
            readObjectStateNode(object, default_network, default_slave, specs_in, currentRefresh, name, state);
        }
    } else if (state.IsSequence()) {
        std::string name;
        for(size_t i = 0; i < state.size(); i++) {
            const YAML::Node& regdata = state[i];
            if (ConfigTools::readOptionalValue<std::string>(name, regdata, "name"))
                is_unnamed = false;
            else if (!is_unnamed)
                throw ConfigurationException(regdata.Mark(), "missing name attribute");
            const YAML::Node& converter = state["converter"];
            readObjectStateNode(object, default_network, default_slave, specs_in, currentRefresh, name, regdata, true);
        }
    }

    //TODO merge with future modbus poll groups
    for(auto& sin: specs_in) {
        std::vector<MsgRegisterPollSpecification>::iterator spec_it = std::find_if(
            specs_out.begin(), specs_out.end(),
            [&sin](const MsgRegisterPollSpecification& s) -> bool { return s.mNetworkName == sin.mNetworkName; }
            );

        if (spec_it == specs_out.end()) {
            specs_out.insert(specs_out.begin(), sin);
        } else {
            //merge overlapping read grups
            spec_it->merge(sin.mRegisters);
        }
    }
}

MqttObjectCommand
ModMqtt::readObjectCommand(const YAML::Node& node, const std::string& default_network, int default_slave) {
    std::string name = ConfigTools::readRequiredString(node, "name");
    // TODO add "registers" section
    // that will store multiple registers
    // and allow to update modbus slaves register by register
    // same as in state
    RegisterConfigName rname(node, default_network, default_slave);
    RegisterType rType = parseRegisterType(node);
    MqttObjectCommand::PayloadType pType = parsePayloadType(node);
    int count = 1;
    ConfigTools::readOptionalValue<int>(count, node, "count");

    MqttObjectCommand cmd(
        name,
        MqttObjectRegisterIdent(
            rname.mNetworkName,
            rname.mSlaveId,
            rType,
            rname.mRegisterNumber
            ),
        pType,
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
        [&pluginName](const boost::shared_ptr<ConverterPlugin>& plugin)
            -> bool { return plugin->getName() == pluginName; }

    );
    if (it == mConverterPlugins.end()) {
        return nullptr;
    }

    std::shared_ptr<DataConverter> ret((*it)->getConverter(converterName));

    return ret;
}

void
ModMqtt::readObjectStateNode(
    MqttObject& object,
    const std::string& default_network,
    int default_slave,
    std::vector<MsgRegisterPollSpecification>& specs_out,
    std::stack<std::chrono::milliseconds>& currentRefresh,
    const std::string& stateName,
    const YAML::Node& node,
    bool isListMember
) {

    int count = 1;
    const YAML::Node& countNode = node["count"];
    if (countNode.IsDefined()) {
        if (isListMember)
            throw ConfigurationException(node.Mark(), "state register list entry cannot contain count");
        count = ConfigTools::readRequiredValue<int>(node, "count");
    }

    MqttObjectRegisterIdent first_ident = updateSpecification(currentRefresh, default_network, default_slave, specs_out, node, count);
    const YAML::Node& converter = node["converter"];
    std::shared_ptr<DataConverter> conv;
    if (converter.IsDefined()) {
        conv = createConverter(converter);
    }
    int end_idx = first_ident.mRegisterNumber + count;
    for(int i = first_ident.mRegisterNumber; i < end_idx; i++) {
        first_ident.mRegisterNumber = i;
        object.mState.addRegister(stateName, first_ident, conv);
    }
}

void
ModMqtt::readObjectAvailability(
    MqttObject& object,
    const std::string& default_network,
    int default_slave,
    std::vector<MsgRegisterPollSpecification>& specs_out,
    std::stack<std::chrono::milliseconds>& currentRefresh,
    const YAML::Node& availability)
{
    if (!availability.IsDefined())
        return;
    if (availability.IsMap()) {
        MqttObjectRegisterIdent ident = updateSpecification(currentRefresh, default_network, default_slave, specs_out, availability);
        uint16_t availValue = ConfigTools::readRequiredValue<uint16_t>(availability, "available_value");
        object.mAvailability.addRegister(ident, availValue);
    } else if (availability.IsSequence()) {
        for(size_t i = 0; i < availability.size(); i++) {
            const YAML::Node& regdata = availability[i];
            MqttObjectRegisterIdent ident = updateSpecification(currentRefresh, default_network, default_slave, specs_out, regdata);
            uint16_t availValue = ConfigTools::readRequiredValue<uint16_t>(availability, "available_value");
            object.mAvailability.addRegister(ident, availValue);
        }
    }
}


void
ModMqtt::readObjectCommands(
    MqttObject& object,
    const std::string& default_network,
    int default_slave,
    const YAML::Node& commands
) {
    if (!commands.IsDefined())
        return;
    if (commands.IsMap()) {
        object.mCommands.push_back(readObjectCommand(commands, default_network, default_slave));
    } else if (commands.IsSequence()) {
        for(size_t i = 0; i < commands.size(); i++) {
            const YAML::Node& cmddata = commands[i];
            object.mCommands.push_back(readObjectCommand(cmddata, default_network, default_slave));
        }
    }
}

std::vector<MsgRegisterPollSpecification>
ModMqtt::initObjects(const YAML::Node& config)
{
    std::vector<MsgRegisterPollSpecification> specs_out;
    std::vector<MqttObjectCommand> commands;
    std::vector<MqttObject> objects;

    auto defaultRefresh = std::chrono::milliseconds(5000);
    std::stack<std::chrono::milliseconds> currentRefresh;
    currentRefresh.push(defaultRefresh);

    const YAML::Node& mqtt = config["mqtt"];
    if (!mqtt.IsDefined())
        throw ConfigurationException(config.Mark(), "mqtt section is missing");

    bool hasGlobalRefresh = parseAndAddRefresh(currentRefresh, mqtt);

    const YAML::Node& config_objects = mqtt["objects"];
    if (!config_objects.IsDefined())
        throw ConfigurationException(mqtt.Mark(), "objects section is missing");

    if (!config_objects.IsSequence())
        throw ConfigurationException(mqtt.Mark(), "mqtt.objects must be a list");

    for(std::size_t i = 0; i < config_objects.size(); i++) {
        const YAML::Node& objdata = config_objects[i];
        MqttObject object(objdata);

        BOOST_LOG_SEV(log, Log::debug) << "processing object " << object.getTopic();

        std::string default_network;
        int default_slave = -1;
        ConfigTools::readOptionalValue<std::string>(default_network, objdata, "network");
        ConfigTools::readOptionalValue<int>(default_slave, objdata, "slave");

        bool hasObjectRefresh = parseAndAddRefresh(currentRefresh, objdata);

        readObjectState(object, default_network, default_slave, specs_out, currentRefresh, objdata["state"]);
        readObjectAvailability(object, default_network, default_slave, specs_out, currentRefresh, objdata["availability"]);
        readObjectCommands(object, default_network, default_slave, objdata["commands"]);

        if (hasObjectRefresh)
            currentRefresh.pop();

        objects.push_back(object);
        BOOST_LOG_SEV(log, Log::debug) << "object for topic " << object.getTopic() << " created";
    }
    if (hasGlobalRefresh)
        currentRefresh.pop();

    mMqtt->setObjects(objects);
    BOOST_LOG_SEV(log, Log::debug) << "Finished reading config_objects specification";
    return specs_out;
}

MqttObjectRegisterIdent
ModMqtt::updateSpecification(
    std::stack<std::chrono::milliseconds>& currentRefresh,
    const std::string& default_network,
    int default_slave,
    std::vector<MsgRegisterPollSpecification>& specs,
    const YAML::Node& data,
    int count)
{
    const RegisterConfigName rname(data, default_network, default_slave);

    bool hasRefresh = parseAndAddRefresh(currentRefresh, data);

    MsgRegisterPoll poll(rname.mRegisterNumber, count);
    poll.mRegisterType = parseRegisterType(data);
    poll.mSlaveId = rname.mSlaveId;
    poll.mRefreshMsec = currentRefresh.top();

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

    if (hasRefresh)
        currentRefresh.pop();

    return MqttObjectRegisterIdent(rname.mNetworkName, rname.mSlaveId, poll.mRegisterType, poll.mRegister);
}

void ModMqtt::start() {
    std::signal(SIGTERM, signal_handler);

    // mosquitto does not use reconnect_delay_set
    // when doing inital connection. We also do not want to
    // process queues before connection to mqtt broker is
    // established - this will cause availability messages to be dropped.

    // TODO if broker is down and modbus is up then mQueues will grow forever and
    // memory allocated by queues will never be released. Add MsgStartPolling?
    BOOST_LOG_SEV(log, Log::debug) << "Performing initial connection to mqtt broker";
    do {
        mMqtt->start();
        if (mMqtt->isConnected()) {
            BOOST_LOG_SEV(log, Log::debug) << "Broker connected, entering main loop";
            break;
        }
        waitForSignal();
    } while(gSignalStatus == -1);

    while(mMqtt->isStarted()) {
        if (gSignalStatus == -1) {
            waitForQueues();
            //BOOST_LOG_SEV(log, Log::debug) << "Processing modbus queues";
            processModbusMessages();
        } else if (gSignalStatus > 0) {
            int currentSignal = gSignalStatus;
            gSignalStatus = -1;
            if (currentSignal == SIGTERM) {
                BOOST_LOG_SEV(log, Log::info) << "Got SIGTERM, exiting…";
                break;
            } else if (currentSignal == SIGHUP) {
                //TODO reload configuration, reconnect broker and
                //create new list of modbus clients if needed
            }
            currentSignal = -1;
        } else if (gSignalStatus == 0) {
            BOOST_LOG_SEV(log, Log::info) << "Got stop request, exiting…";
            break;
        }
    };

    BOOST_LOG_SEV(log, Log::info) << "Stopping modbus clients";
    for(std::vector<std::shared_ptr<ModbusClient>>::iterator client = mModbusClients.begin();
        client < mModbusClients.end(); client++)
    {
        (*client)->stop();
    }

    if (mMqtt->isConnected()) {
        BOOST_LOG_SEV(log, Log::info) << "Publishing availability status 0 for all registers";
        for(std::vector<std::shared_ptr<ModbusClient>>::iterator client = mModbusClients.begin();
            client < mModbusClients.end(); client++)
        {
            mMqtt->processModbusNetworkState((*client)->mName, false);
        }
    }
    BOOST_LOG_SEV(log, Log::debug) << "Shutting down mosquitto client";
    // If connected, then shutdown()
    // will send disconnection request to mqtt broker.
    // After disconnection mMqtt will notify global queue mutex
    // Otherwise we are already stopped.
    mMqtt->shutdown();
    if (mMqtt->isStarted()) {
        BOOST_LOG_SEV(log, Log::debug) << "Waiting for disconnection event";
        waitForQueues();
    }
    BOOST_LOG_SEV(log, Log::info) << "Shutdown finished";
}

void
ModMqtt::stop() {
    BOOST_LOG_SEV(log, Log::debug) << "Sending stop request to ModMqtt server";
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
                mMqtt->processRegisterValues((*client)->mName, *val);
            } else if (item.isSameAs(typeid(MsgRegisterReadFailed))) {
                std::unique_ptr<MsgRegisterReadFailed> val(item.getData<MsgRegisterReadFailed>());
                mMqtt->processRegistersOperationFailed((*client)->mName, *val);
            } else if (item.isSameAs(typeid(MsgRegisterWriteFailed))) {
                std::unique_ptr<MsgRegisterWriteFailed> val(item.getData<MsgRegisterWriteFailed>());
                mMqtt->processRegistersOperationFailed((*client)->mName, *val);
            } else if (item.isSameAs(typeid(MsgModbusNetworkState))) {
                std::unique_ptr<MsgModbusNetworkState> val(item.getData<MsgModbusNetworkState>());
                mMqtt->processModbusNetworkState(val->mNetworkName, val->mIsUp);
            }
        }
    }
}

bool
ModMqtt::hasConverterPlugin(const std::string& name) const {
    auto it = std::find_if(
        mConverterPlugins.begin(),
        mConverterPlugins.end(),
        [&name](const boost::shared_ptr<ConverterPlugin>& plugin)
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
