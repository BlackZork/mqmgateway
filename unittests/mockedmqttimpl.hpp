#pragma once

#include <mutex>
#include <condition_variable>
#include <map>
#include <set>

#include "libmodmqttsrv/imqttimpl.hpp"
#include "libmodmqttsrv/logging.hpp"

class MockedMqttException : public modmqttd::ModMqttException {
    public:
        MockedMqttException(const std::string& what) : ModMqttException(what) {}
};

class MockedMqttImpl : public modmqttd::IMqttImpl {
    class MqttValue {
        public:
            MqttValue() {}
            MqttValue(const void* v, int l) {
                copyData(v, l);
            }
            MqttValue(const MqttValue& from) {
                copyData(from.val, from.len);
                publishCount = from.publishCount;
            }
            MqttValue& operator=(const MqttValue& other) {
                copyData(other.val, other.len);
                publishCount = other.publishCount;
                return *this;
            }
            ~MqttValue() {
                if (val)
                    free(val);
            }
            char* val = NULL;
            int len = 0;
            int publishCount = 0;
        private:
            void copyData(const void* v, int l) {
                if (val)
                    free(val);
                val = (char*)malloc(l+1);
                memcpy(val, v, l);
                val[l] = '\0';
                len = l;
            };
    };
    public:
        virtual void init(modmqttd::MqttClient* owner, const char* clientId);
        virtual void connect(const modmqttd::MqttBrokerConfig& config);
        virtual void reconnect();
        virtual void disconnect();
        virtual void stop();

        virtual void subscribe(const char* topic);
        virtual void publish(const char* topic, int len, const void* data, bool retain);

        virtual void on_disconnect(int rc);
        virtual void on_connect(int rc);
        virtual void on_log(int level, const char* message);

        //unit test tools
        bool waitForSubscription(const char* topic, std::chrono::milliseconds timeout);
        bool waitForPublish(const char* topic, std::chrono::milliseconds timeout);
        std::string waitForFirstPublish(std::chrono::milliseconds timeout);
        int getPublishCount(const char* topic);
        bool hasTopic(const char* topic);
        std::string mqttValue(const char* topic);
        bool mqttNullValue(const char* topic);
        //returns current value on timeout
        std::string waitForMqttValue(const char* topic, const char* expected, std::chrono::milliseconds timeout);
        //clear all topics and simulate broker disconnection
        void resetBroker();
    private:
        modmqttd::MqttClient* mOwner;
        boost::log::sources::severity_logger<modmqttd::Log::severity> log;

        std::map<std::string, MqttValue> mTopics;
        std::set<std::string> mSubscriptions;

        //contains all topics published before waitForPublish/waitForFirstPublish
        //call. Map value contains mqtt publish count
        std::map<std::string, int> mPublishedTopics;

        std::mutex mMutex;
        std::condition_variable mCondition;
};
