#pragma once

#include <mosquittopp.h>
#include "config.hpp"
#include "common.hpp"
#include "mqttobject.hpp"
#include "modbus_client.hpp"
#include "imqttimpl.hpp"

namespace modmqttd {

class MqttClient;

class Mosquitto : public mosqpp::mosquittopp, public IMqttImpl {
    public:
        virtual void init(MqttClient* owner, const char* clientId);
        virtual void connect(const MqttBrokerConfig& config);
        virtual void stop() { loop_stop(); }

        virtual void reconnect() { mosquittopp::reconnect(); }
        virtual void disconnect() { mosquittopp::disconnect(); }

        virtual void subscribe(const char* topic);
        virtual void publish(const char* topic, int len, const void* data);

        virtual void on_disconnect(int rc);
        virtual void on_connect(int rc);
        virtual void on_log(int level, const char* message);
        virtual void on_message(const struct mosquitto_message *message);
    private:
        MqttClient* mOwner;
        boost::log::sources::severity_logger<Log::severity> log;

        const char* returnCodeToStr(int code);
        void throwOnCriticalError(int code);
};

}
