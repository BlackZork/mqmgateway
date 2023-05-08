#pragma once
#include <vector>
#include <stack>
#include <mutex>
#include <condition_variable>

#include "libmodmqttconv/converterplugin.hpp"

#include "common.hpp"
#include "modbus_client.hpp"
#include "mosquitto.hpp"
#include "modbus_messages.hpp"
#include "mqttobject.hpp"
#include "imodbuscontext.hpp"


namespace modmqttd {

// global shared lock for modbus threads and main loop
// to wait on empty queues and posix singals processing
extern std::mutex gQueueMutex;
extern std::condition_variable gHasMessagesCondition;
extern bool gHasMessagesFlag;
void notifyQueues();

class ModMqtt {
    public:
        static void setModbusContextFactory(const std::shared_ptr<IModbusFactory>& factory);
        static IModbusFactory& getModbusFactory() { return *mModbusFactory; }

        boost::log::sources::severity_logger<Log::severity> log;
        ModMqtt();
        void addConverterPath(const std::string& path) { mConverterPaths.push_back(path); }
        void init(const std::string& configPath);
        void init(const YAML::Node& config);
        void start();
        /**
            Stop server. Can be called only from controlling thread
            Used by unit tests only
        */
        void stop();
        void waitForQueues();
        void setMqttFinished() { mMqttFinished = true; }

        void setMqttImplementation(const std::shared_ptr<IMqttImpl>& impl);
        ~ModMqtt();
    private:
        static std::shared_ptr<IModbusFactory> mModbusFactory;

        std::shared_ptr<MqttClient> mMqtt;
        std::vector<std::shared_ptr<ModbusClient>> mModbusClients;
        std::vector<MsgRegisterPollSpecification> mSpecs;

        std::vector<boost::shared_ptr<ConverterPlugin>> mConverterPlugins;

        void initServer(const YAML::Node& config);
        void initBroker(const YAML::Node& config);
        void initModbusClients(const YAML::Node& config);
        void initObjects(const YAML::Node& config);
        void waitForSignal();

        MqttObjectRegisterIdent updateSpecification(std::stack<int>& currentRefresh, const std::string& default_network, int default_slave, const YAML::Node& data);
        bool parseAndAddRefresh(std::stack<int>& values, const YAML::Node& data);
        void readObjectState(MqttObject& object, const std::string& default_network, int default_slave, std::stack<int>& currentRefresh, const YAML::Node& state);
        void readObjectStateNode(MqttObject& object, const std::string& default_network, int default_slave, std::stack<int>& currentRefresh, const std::string& stateName, const YAML::Node& node);
        void readObjectAvailability(MqttObject& object, const std::string& default_network, int default_slave, std::stack<int>& currentRefresh, const YAML::Node& availability);
        void readObjectCommands(MqttObject& object, const std::string& default_network, int default_slave, const YAML::Node& commands);
        void processModbusMessages();

        bool hasConverterPlugin(const std::string& name) const;
        boost::shared_ptr<ConverterPlugin> initConverterPlugin(const std::string& name);
        std::shared_ptr<IStateConverter> createConverterInstance(const std::string plugin, const std::string& converter) const;
        std::shared_ptr<IStateConverter> createConverter(const YAML::Node& data) const;


        bool mMqttFinished = false;

        std::vector<std::string> mConverterPaths;
};

}
