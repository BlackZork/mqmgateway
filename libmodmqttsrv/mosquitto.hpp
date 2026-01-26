#pragma once

#include <mosquitto.h>
#include "config.hpp"
#include "common.hpp"
#include "mqttobject.hpp"
#include "modbus_client.hpp"
#include "imqttimpl.hpp"

namespace modmqttd {

class MqttClient;

class Mosquitto : public IMqttImpl {
    public:
        static void libInit();
        static void libCleanup();

        Mosquitto();
        virtual void init(MqttClient* owner, const char* clientId);
        virtual void connect(const MqttBrokerConfig& config);
        virtual void stop();

        virtual void reconnect();
        virtual void disconnect();

        virtual void subscribe(const char* topic);
        virtual void publish(const char* topic, int len, const void* data, bool retain);

        virtual void on_disconnect(int rc);
        virtual void on_connect(int rc);
        virtual void on_log(int level, const char* message);
        virtual void on_message(const struct mosquitto_message *message);
        virtual ~Mosquitto();
    private:
        mosquitto *mMosq = NULL;
        MqttClient* mOwner;

        const char* returnCodeToStr(int code);
        void throwOnCriticalError(int code);
};

}
