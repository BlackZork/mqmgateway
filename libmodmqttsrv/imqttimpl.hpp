#pragma once

#include "config.hpp"

namespace modmqttd {

class MqttClient;
class MqttPublishProps;

/**
    Abstract base class for mqtt communication library implementation
*/
class IMqttImpl {
    public:
        virtual void init(MqttClient* owner, const char* clientId) = 0;
        virtual void connect(const MqttBrokerConfig& config) = 0;
        virtual void reconnect() = 0;
        virtual void disconnect() = 0;
        virtual void stop() = 0;

        virtual void subscribe(const char* topic) = 0;
        virtual void publish(const char* topic, int len, const void* data) = 0;
        virtual void publish(const char* topic, int len, const void* data, const MqttPublishProps& md) = 0;

        virtual void on_disconnect(int rc) = 0;
        virtual void on_connect(int rc)= 0;
        virtual void on_log(int level, const char* message)= 0;
        virtual ~IMqttImpl() {};
};

}
