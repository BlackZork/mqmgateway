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
void notifyQueues();


class ModMqtt {
    public:
        static void setModbusContextFactory(const std::shared_ptr<IModbusFactory>& factory);
        static IModbusFactory& getModbusFactory() { return *mModbusFactory; }

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
        struct ModbusInitData {
            std::vector<MsgRegisterPollSpecification> mPollSpecification;

            //network -> map(slave_id, slave_name)
            std::map<std::string, std::map<int, std::string>> mSlaveNames;

            std::string getSlaveName(const std::string& pNetwork, int pSlaveId) const {
                auto nit = mSlaveNames.find(pNetwork);
                if (nit == mSlaveNames.end())
                    return std::string();

                auto sit = nit->second.find(pSlaveId);
                if (sit == nit->second.end())
                    return std::string();

                return sit->second;
            }
        };

        static std::shared_ptr<IModbusFactory> mModbusFactory;

        std::shared_ptr<MqttClient> mMqtt;
        std::vector<std::shared_ptr<ModbusClient>> mModbusClients;

        std::vector<std::shared_ptr<ConverterPlugin>> mConverterPlugins;

        void initServer(const YAML::Node& config);
        void initBroker(const YAML::Node& config);
        ModbusInitData initModbusClients(const YAML::Node& config);
        std::vector<MqttObject> initObjects(const YAML::Node& config, const ModbusInitData& modbusData, std::vector<MsgRegisterPollSpecification>& pSpecsOut);
        void waitForSignal();

        MqttObjectRegisterIdent updateSpecification(
            const YAML::Node& pData,
            int pRegisterCount,
            const std::chrono::milliseconds& pCurrentRefresh,
            const std::string& pDefaultNetwork,
            int pDefaultSlave,
            PublishMode pCurrentMode,
            std::vector<MsgRegisterPollSpecification>& specs
        );

        bool parseAndAddRefresh(std::stack<std::chrono::milliseconds>& values, const YAML::Node& data);

        MqttObject parseObject(
            const YAML::Node& pData,
            const std::string& pDefaultNetwork,
            int pDefaultSlave,
            const std::string& pSlaveName,
            std::chrono::milliseconds pDefaultRefresh,
            PublishMode pDefaultPublishMode,
            std::vector<MsgRegisterPollSpecification>& pSpecsOut
        );

        MqttObjectDataNode parseObjectDataNode(
            const YAML::Node& pNode,
            const std::string& pDefaultNetwork,
            int pDefaultSlave,
            std::chrono::milliseconds refresh,
            PublishMode pMode,
            std::chrono::milliseconds& pEveryPollRefreshOut,
            std::vector<MsgRegisterPollSpecification>& pSpecs
        );

        MqttObjectAvailability parseAvailability(
            const YAML::Node& pData,
            const std::string& pDefaultNetwork,
            int pDefaultSlave,
            std::chrono::milliseconds pDefaultRefresh,
            std::vector<MsgRegisterPollSpecification>& pSpecsOut
        );

        int parseObjectCommands(
            const std::string& pTopicPrefix,
            int nextCommandId,
            const YAML::Node& pCommands,
            const std::string& pDefaultNetwork,
            int pDefaultSlave
        );

        std::vector<modmqttd::MsgRegisterPoll> readModbusPollGroups(const std::string& modbus_network, int default_slave, const YAML::Node& groups);
        void processModbusMessages();

        MqttObjectCommand parseObjectCommand(const std::string& pTopicPrefix, int nextCommandId, const YAML::Node& node, const std::string& default_network, int default_slave);

        bool hasConverterPlugin(const std::string& name) const;
        std::shared_ptr<ConverterPlugin> initConverterPlugin(const std::string& name);

        std::shared_ptr<DataConverter> createConverterInstance(const std::string plugin, const std::string& converter) const;
        std::shared_ptr<DataConverter> createConverter(const YAML::Node& data) const;


        bool mMqttFinished = false;

        std::vector<std::string> mConverterPaths;
};

}
