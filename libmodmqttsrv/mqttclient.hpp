#pragma once

#include <map>
#include <vector>

#include "config.hpp"
#include "common.hpp"
#include "mqttobject.hpp"
#include "modbus_client.hpp"
#include "imqttimpl.hpp"
#include "default_command_converter.hpp"
#include "pending_rpc_request.hpp"

namespace modmqttd {

class ModMqtt;

class MqttClient {
    public:
        typedef std::map<MqttObjectRegisterIdent, std::vector<std::shared_ptr<MqttObject>>, MqttObjectRegisterIdent::Compare> MqttPollObjMap;
        typedef std::map<int, std::vector<std::shared_ptr<MqttObject>>> MqttCmdObjMap;
        typedef std::map<int, PendingRpcRequest> MqttRpcPendingMap;

        enum State {
            DISCONNECTED,
            CONNECTING,
            CONNECTED,
            DISCONNECTING
        };

        MqttClient(ModMqtt& modmqttd);
        void setClientId(const std::string& clientId);
        void setBrokerConfig(const MqttBrokerConfig& config);
        void setModbusClients(const std::vector<std::shared_ptr<ModbusClient>>& clients) { mModbusClients = clients; }
        void start(); // TODO throw(MosquittoException) - deprecated?;
        bool isStarted() { return mIsStarted; }
        void shutdown();
        bool isConnected() const { return mConnectionState == State::CONNECTED; }
        void reconnect() { mMqttImpl->reconnect(); }
        void setObjects(const MqttPollObjMap& pObjects) { mObjects = pObjects; };
        void setCommandObjects(const MqttCmdObjMap& pCmdObjects) { mCommandObjects = pCmdObjects; }
        void setRpcMode(RpcMode mode) { mRpcMode = mode; }

        void addCommand(const MqttObjectCommand& pCommand);
        const std::map<std::string, MqttObjectCommand>& getCommands() const { return mCommands; }

        void processRegisterValues(const std::string& modbusNetworkName, const MsgRegisterValues& values);
        void processRegistersOperationFailed(const std::string& modbusNetworkName, const ModbusMessageBase& values);
        void processModbusNetworkState(const std::string& modbusNetworkName, bool isUp);
        void publishRpcError(const std::string& modbusNetworkName, const ModbusMessageBase& slaveData);

        // mqtt communication callbacks
        void onDisconnect();
        void onConnect();
        void onMessage(const char* topic, const void* payload, int payload_len,
            const char* responseTopic = nullptr,
            const std::shared_ptr<void>& correlationData = nullptr, int correlationLen = 0);
        void onPublish(int messageId) {}

        // for unit tests
        void setMqttImplementation(const std::shared_ptr<IMqttImpl>& impl) { mMqttImpl = impl; }

    private:
        // publish all data after broker is reconnected
        void publishAll();
        void publishState(const std::shared_ptr<MqttObject>&, bool force = false);
        void publishAvailabilityChange(const MqttObject& obj);

        void handleRpcRequest(const void* payload, int payloadlen,
            const char* responseTopic,
            const std::shared_ptr<void>& correlationData, int correlationLen);
        void publishRpcResponse(const std::string& networkName, const MsgRegisterValues& values);
        void publishRpcError(const std::string& responseTopic,
            const CorrelationData& correlationData, const std::string& errorMsg);

        std::shared_ptr<IMqttImpl> mMqttImpl;

        void subscribeToCommandTopic(const std::string& objectName, const MqttObjectCommand& cmd);

        ModMqtt& mOwner;
        MqttBrokerConfig mBrokerConfig;

        void checkAvailabilityChange(MqttObject& object, const MqttObjectRegisterIdent& ident, uint16_t value);
        std::map<std::string, MqttObjectCommand>::const_iterator findCommand(const char* topic) const;

        std::vector<std::shared_ptr<ModbusClient>> mModbusClients;

        // TODO check in which thread context mConnectionState is changed.
        // Now it looks like callbacks use mosquitto internal thread and ModMqtt main thread.
        State mConnectionState = State::DISCONNECTED;
        bool mIsStarted = false;
        /**
         * Assuming that PollGroups do not overlap hold separate list
         * per poll group ident. This way for each MsgRegisterValues we can update
         * objects from single list only.
         * MqttObject can be a member of multiple lists on this map
         */
        MqttPollObjMap mObjects;

        /**
         * Direct relation between command and objects that poll the
         * same registers
         */
        MqttCmdObjMap mCommandObjects;

        std::map<std::string, MqttObjectCommand> mCommands;

        DefaultCommandConverter mDefaultConverter;

        RpcMode mRpcMode = RpcMode::Disabled;
        std::string mRpcRequestTopic;
        int mNextRpcId = 0;
        MqttRpcPendingMap mPendingRpc;
};

}
