#pragma once

#include <mosquitto.h>
#include <mqtt_protocol.h>
#include "config.hpp"
#include "common.hpp"
#include "mqttobject.hpp"
#include "modbus_client.hpp"
#include "imqttimpl.hpp"

namespace modmqttd {

class MqttClient;
class MqttPublishProps;

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
        virtual void publish(const char* topic, int len, const void* data);
        virtual void publish(const char* topic, int len, const void* data, const MqttPublishProps& md);

        virtual void on_disconnect(int rc);
        virtual void on_connect(int rc);
        virtual void on_log(int level, const char* message);
        virtual void on_message(const struct mosquitto_message *message, const mosquitto_property *props);
        virtual ~Mosquitto();
    private:
        mosquitto *mMosq = NULL;
        MqttClient* mOwner;
        boost::log::sources::severity_logger<Log::severity> log;

        const char* returnCodeToStr(int code);
        void throwOnCriticalError(int code);
};

}
