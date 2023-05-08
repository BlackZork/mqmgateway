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
#include "conv_name_parser.hpp"

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

            std::regex re("^([a-zA-Z0-9]+\\.)?([0-9]+\\.)?((0[xX])?[0-9a-fA-F]+)$");
            std::cmatch matches;

            if (!std::regex_match(str.c_str(), matches, re))
                throw ConfigurationException(data["register"].Mark(), "Invalid register specification");

            std::string network = matches[1];
            if (!network.empty()) {
                network.pop_back();
                mNetworkName = network;
            } else if (default_slave != -1) {
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

            mRegisterAddress = std::stoi(matches[3], nullptr, 0);
        };
        std::string mNetworkName;
        int mSlaveId = 0;
        int mRegisterAddress;
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

MqttObjectCommand
readCommand(const YAML::Node& node, const std::string& default_network, int default_slave) {
    std::string name = ConfigTools::readRequiredString(node, "name");
    RegisterConfigName rname(node, default_network, default_slave);
    RegisterType rType = parseRegisterType(node);
    MqttObjectCommand::PayloadType pType = parsePayloadType(node);
    return MqttObjectCommand(
        name,
        MqttObjectRegisterIdent(
            rname.mNetworkName,
            rname.mSlaveId,
            rType,
            rname.mRegisterAddress
            ),
        pType
    );
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
    initObjects(config);

    initModbusClients(config);

    for(std::vector<MsgRegisterPollSpecification>::iterator sit = mSpecs.begin(); sit != mSpecs.end(); sit++) {
        const std::string& netname = sit->mNetworkName;
        std::vector<std::shared_ptr<ModbusClient>>::iterator client = std::find_if(
            mModbusClients.begin(), mModbusClients.end(),
            [&netname](const std::shared_ptr<ModbusClient>& client) -> bool { return client->mName == netname; }
        );
        if (client == mModbusClients.end()) {
            BOOST_LOG_SEV(log, Log::error) << "Modbus client for " << netname << " not initialized, ignoring specification";
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

void ModMqtt::initModbusClients(const YAML::Node& config) {
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

    for(std::size_t i = 0; i < networks.size(); i++) {
        ModbusNetworkConfig modbus_config(networks[i]);

        std::shared_ptr<ModbusClient> modbus(new ModbusClient());
        modbus->init(modbus_config);
        mModbusClients.push_back(modbus);
    }
    mMqtt->setModbusClients(mModbusClients);
    BOOST_LOG_SEV(log, Log::debug) << "Modbus clients initialized";
}

bool
ModMqtt::parseAndAddRefresh(std::stack<int>& values, const YAML::Node& data) {
    std::string str;
    if (!ConfigTools::readOptionalValue<std::string>(str, data, "refresh"))
        return false;

    std::regex re("([0-9]+)(ms|s|min)");
    std::cmatch matches;

    if (!std::regex_match(str.c_str(), matches, re))
        throw ConfigurationException(data["refresh"].Mark(), "Invalid refresh time");

    int value = std::stoi(matches[1]);
    std::string unit = matches[2];
    if (unit == "s")
        value *= 1000;
    else if (unit == "min")
        value *= 1000 * 60;

    values.push(value);
    return true;
}

void
ModMqtt::readObjectState(
    MqttObject& object,
    const std::string& default_network,
    int default_slave,
    std::stack<int>& currentRefresh,
    const YAML::Node& state)
{
    if (!state.IsDefined())
        return;

    bool is_unnamed = true;
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
                readObjectStateNode(object, default_network, default_slave, currentRefresh, name, regdata);
            };
        } else {
            //single named register
            readObjectStateNode(object, default_network, default_slave, currentRefresh, name, state);
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
            readObjectStateNode(object, default_network, default_slave, currentRefresh, name, regdata);
        }
    }
}

std::shared_ptr<IStateConverter>
ModMqtt::createConverter(const YAML::Node& node) const {
    if (!node.IsScalar())
        throw ConfigurationException(node.Mark(), "converter must be a string");
    std::string line = ConfigTools::readRequiredValue<std::string>(node);

    try {
        ConverterSpecification spec(ConverterNameParser::parse(line));

        std::shared_ptr<IStateConverter> conv = createConverterInstance(spec.plugin, spec.converter);
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

std::shared_ptr<IStateConverter>
ModMqtt::createConverterInstance(const std::string pluginName, const std::string& converter) const {
    auto it = std::find_if(
        mConverterPlugins.begin(),
        mConverterPlugins.end(),
        [&pluginName](const boost::shared_ptr<ConverterPlugin>& plugin)
            -> bool { return plugin->getName() == pluginName; }

    );
    if (it == mConverterPlugins.end()) {
        return nullptr;
    }

    std::shared_ptr<IStateConverter> ret((*it)->getStateConverter(converter));
    return ret;
}

void
ModMqtt::readObjectStateNode(
    MqttObject& object,
    const std::string& default_network,
    int default_slave,
    std::stack<int>& currentRefresh,
    const std::string& stateName,
    const YAML::Node& node
) {
    MqttObjectRegisterIdent ident = updateSpecification(currentRefresh, default_network, default_slave, node);
    const YAML::Node& converter = node["converter"];
    std::shared_ptr<IStateConverter> conv;
    if (converter.IsDefined()) {
        conv = createConverter(converter);
    }
    object.mState.addRegister(stateName, ident, conv);
}

void
ModMqtt::readObjectAvailability(
    MqttObject& object,
    const std::string& default_network,
    int default_slave,
    std::stack<int>& currentRefresh,
    const YAML::Node& availability)
{
    if (!availability.IsDefined())
        return;
    if (availability.IsMap()) {
        MqttObjectRegisterIdent ident = updateSpecification(currentRefresh, default_network, default_slave, availability);
        uint16_t availValue = ConfigTools::readRequiredValue<uint16_t>(availability, "available_value");
        object.mAvailability.addRegister(ident, availValue);
    } else if (availability.IsSequence()) {
        for(size_t i = 0; i < availability.size(); i++) {
            const YAML::Node& regdata = availability[i];
            MqttObjectRegisterIdent ident = updateSpecification(currentRefresh, default_network, default_slave, regdata);
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
        object.mCommands.push_back(readCommand(commands, default_network, default_slave));
    } else if (commands.IsSequence()) {
        for(size_t i = 0; i < commands.size(); i++) {
            const YAML::Node& cmddata = commands[i];
            object.mCommands.push_back(readCommand(cmddata, default_network, default_slave));
        }
    }
}

void
ModMqtt::initObjects(const YAML::Node& config)
{
    std::vector<MqttObjectCommand> commands;
    std::vector<MqttObject> objects;

    int defaultRefresh = 5000;
    std::stack<int> currentRefresh;
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

        readObjectState(object, default_network, default_slave, currentRefresh, objdata["state"]);
        readObjectAvailability(object, default_network, default_slave, currentRefresh, objdata["availability"]);
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
}

MqttObjectRegisterIdent
ModMqtt::updateSpecification(
    std::stack<int>& currentRefresh,
    const std::string& default_network,
    int default_slave,
    const YAML::Node& data)
{
    const RegisterConfigName rname(data, default_network, default_slave);

    bool hasRefresh = parseAndAddRefresh(currentRefresh, data);

    MsgRegisterPoll poll;
    poll.mRegister = rname.mRegisterAddress;
    poll.mRegisterType = parseRegisterType(data);
    poll.mSlaveId = rname.mSlaveId;
    poll.mRefreshMsec = currentRefresh.top();

    // find network poll specification or create one
    std::vector<MsgRegisterPollSpecification>::iterator spec_it = std::find_if(
        mSpecs.begin(), mSpecs.end(),
        [&rname](const MsgRegisterPollSpecification& s) -> bool { return s.mNetworkName == rname.mNetworkName; }
        );

    if (spec_it == mSpecs.end()) {
        BOOST_LOG_SEV(log, Log::debug) << "Creating new register specification for network " << rname.mNetworkName;
        mSpecs.insert(mSpecs.begin(), MsgRegisterPollSpecification(rname.mNetworkName));
        spec_it = mSpecs.begin();
    }

    // add new register poll or update refresh time on existing one
    std::vector<MsgRegisterPoll>::iterator reg_it = std::find_if(
        spec_it->mRegisters.begin(), spec_it->mRegisters.end(),
        [&rname, &poll](const MsgRegisterPoll& r) -> bool {
            return r.mRegister == rname.mRegisterAddress
                    && r.mRegisterType == poll.mRegisterType
                    && r.mSlaveId == poll.mSlaveId;
            }
    );

    if (reg_it == spec_it->mRegisters.end()) {
        BOOST_LOG_SEV(log, Log::debug) << "Adding new register " << poll.mRegister <<
        " type=" << poll.mRegisterType << " refresh=" << poll.mRefreshMsec
        << " slaveId=" << rname.mSlaveId << " on network " << rname.mNetworkName;
        spec_it->mRegisters.push_back(poll);
    } else {
        //set the shortest poll period of all occurrences in config file
        if (reg_it->mRefreshMsec > poll.mRefreshMsec) {
            reg_it->mRefreshMsec = poll.mRefreshMsec;
            BOOST_LOG_SEV(log, Log::debug) << "Setting refresh " << poll.mRefreshMsec << " on existing register " << poll.mRegister;
        }
    }

    if (hasRefresh)
        currentRefresh.pop();

    return MqttObjectRegisterIdent(rname.mNetworkName, rname.mSlaveId, poll.mRegisterType, poll.mRegister);
}

void ModMqtt::start() {
    std::signal(SIGTERM, signal_handler);

    // mosquitto does not use reconnect_delay_set
    // when doing inital connection. We also do not want to
    // process queues before connection to mqtt broker is
    // estabilished - this will cause availability messages to be dropped.

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
                BOOST_LOG_SEV(log, Log::info) << "Got SIGTERM, exiting....";
                break;
            } else if (currentSignal == SIGHUP) {
                //TODO reload configuratiion, reconect borker and
                //create new list of modbus clients if needed
            }
            currentSignal = -1;
        } else if (gSignalStatus == 0) {
            BOOST_LOG_SEV(log, Log::info) << "Got stop request, exiting....";
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
        BOOST_LOG_SEV(log, Log::info) << "Publishing avaiability status 0 for all registers";
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
            if (item.isSameAs(typeid(MsgRegisterValue))) {
                std::unique_ptr<MsgRegisterValue> val(item.getData<MsgRegisterValue>());
                MqttObjectRegisterIdent ident((*client)->mName, val->mSlaveId, val->mRegisterType, val->mRegisterAddress);
                mMqtt->processRegisterValue(ident, val->mValue);
            } else if (item.isSameAs(typeid(MsgRegisterReadFailed))) {
                std::unique_ptr<MsgRegisterReadFailed> val(item.getData<MsgRegisterReadFailed>());
                MqttObjectRegisterIdent ident((*client)->mName, val->mSlaveId, val->mRegisterType, val->mRegisterAddress);
                mMqtt->processRegisterOperationFailed(ident);
            } else if (item.isSameAs(typeid(MsgRegisterWriteFailed))) {
                std::unique_ptr<MsgRegisterWriteFailed> val(item.getData<MsgRegisterWriteFailed>());
                MqttObjectRegisterIdent ident((*client)->mName, val->mSlaveId, val->mRegisterType, val->mRegisterAddress);
                mMqtt->processRegisterOperationFailed(ident);
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
    while(!gHasMessages)
        gHasMessagesCondition.wait(lock);
    gHasMessages = false;
    lock.unlock();
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
