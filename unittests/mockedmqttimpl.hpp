#pragma once

#include <mutex>
#include <condition_variable>
#include <map>
#include <set>

#include "libmodmqttsrv/imqttimpl.hpp"
#include "libmodmqttsrv/logging.hpp"
#include "libmodmqttsrv/mqttobject.hpp"

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
            MqttValue(const void* v, int l, modmqttd::MqttPublishProps props) 
                :mProps(props) {
                    copyData(v, l);
                }
            MqttValue(const MqttValue& from) {
                copyData(from.val, from.len);
                mProps = from.mProps;
            }
            MqttValue& operator=(const MqttValue& other) {
                copyData(other.val, other.len);
                mProps = other.mProps;
                return *this;
            }
            ~MqttValue() {
                if (val)
                    free(val);
            }
            char* val = NULL;
            int len = 0;
            modmqttd::MqttPublishProps mProps;
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
        virtual void publish(const char* topic, int len, const void* data);
        virtual void publish(const char* topic, int len, const void* data, const modmqttd::MqttPublishProps& md);
        void publish(const char* topic, int len, const void* data, modmqttd::MqttObjectBase::PayloadType payloadType);

        virtual void on_disconnect(int rc);
        virtual void on_connect(int rc);
        virtual void on_log(int level, const char* message);

        //unit test tools
        bool waitForPublish(const char* topic, std::chrono::milliseconds timeout = std::chrono::seconds(1));
        std::string waitForFirstPublish(std::chrono::milliseconds timeout);
        bool hasTopic(const char* topic);
        std::string mqttValue(const char* topic);
        modmqttd::MqttPublishProps mqttValueProps(const char* topic);

        //returns current value on timeout
        std::string waitForMqttValue(const char* topic, const char* expected, std::chrono::milliseconds timeout = std::chrono::seconds(1));
        //clear all topics and simulate broker disconnection
        void resetBroker();
    private:
        modmqttd::MqttClient* mOwner;
        boost::log::sources::severity_logger<modmqttd::Log::severity> log;

        std::map<std::string, MqttValue> mTopics;
        std::set<std::string> mSubscriptions;
        std::map<std::string, int> mPublishedTopics;

        std::mutex mMutex;
        std::condition_variable mCondition;

        bool isTopicCreated(const char* topic) const;
};
