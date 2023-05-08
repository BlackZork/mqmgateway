#pragma once

#include "config.hpp"
#include "common.hpp"
#include "mqttobject.hpp"
#include "modbus_client.hpp"
#include "imqttimpl.hpp"

namespace modmqttd {

class ModMqtt;

class MqttClient {
    public:
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
        void start() ;//TODO throw(MosquittoException) - depreciated?;
        bool isStarted() { return mIsStarted; }
        void shutdown();
        bool isConnected() const { return mConnectionState == State::CONNECTED; }
        void reconnect() { mMqttImpl->reconnect(); }
        void setObjects(const std::vector<MqttObject> objects) { mObjects = objects; };

        //publish all data after broker is reconnected
        void publishAll();
        void publishState(const MqttObject& obj);

        void processRegisterValue(const MqttObjectRegisterIdent& ident, uint16_t value);
        void processRegisterOperationFailed(const MqttObjectRegisterIdent& ident);
        void processModbusNetworkState(const std::string& networkName, bool isUp);
        void publishAvailabilityChange(const MqttObject& obj);

        //mqtt communication callbacks
        void onDisconnect();
        void onConnect();
        void onMessage(const char* topic, const void* payload, int payload_len, const modmqttd::MqttPublishProps& md);

        //for unit tests
        void setMqttImplementation(const std::shared_ptr<IMqttImpl>& impl) { mMqttImpl = impl; }
    private:
        std::shared_ptr<IMqttImpl> mMqttImpl;

        void subscribeToCommandTopic(const std::string& objectName, const MqttObjectBase& cmd);

        boost::log::sources::severity_logger<Log::severity> log;
        ModMqtt& mOwner;
        MqttBrokerConfig mBrokerConfig;

        void checkAvailabilityChange(MqttObject& object, const MqttObjectRegisterIdent& ident, uint16_t value);
        const MqttObjectBase& findCommand(const char* topic) const;

        std::vector<std::shared_ptr<ModbusClient>> mModbusClients;

        // TODO check in which thread context mConnectionState is changed.
        // Now it looks like callbacks use mosquitto internal thread and ModMqtt main thread.
        State mConnectionState = State::DISCONNECTED;
        bool mIsStarted = false;
        std::vector<MqttObject> mObjects;
};

}
