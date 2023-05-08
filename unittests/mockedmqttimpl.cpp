#include "mockedmqttimpl.hpp"
#include "libmodmqttsrv/mqttclient.hpp"
#include "libmodmqttsrv/mqttobject.hpp"

void
MockedMqttImpl::init(modmqttd::MqttClient* owner, const char* clientId) {
    mOwner = owner;
}

void
MockedMqttImpl::connect(const modmqttd::MqttBrokerConfig& config) {
    mOwner->onConnect();
}

void
MockedMqttImpl::reconnect() {
    mOwner->onConnect();
}

void
MockedMqttImpl::disconnect() {
    mOwner->onDisconnect();
}

void
MockedMqttImpl::stop() {
    //nothing to do
}

void
MockedMqttImpl::subscribe(const char* topic) {
    std::unique_lock<std::mutex> lck(mMutex);
    mSubscriptions.insert(topic);
}

void
MockedMqttImpl::publish(const char* topic, int len, const void* data) {
    modmqttd::MqttPublishProps md;
    publish(topic, len, data, md);
}

void
MockedMqttImpl::publish(const char* topic, int len, const void* data, const modmqttd::MqttPublishProps& md) {
    std::unique_lock<std::mutex> lck(mMutex);
    MqttValue v(data, len, md);
    mTopics[topic] = v;
    std::set<std::string>::const_iterator it = mSubscriptions.find(topic);
    if (it != mSubscriptions.end()) {
        mOwner->onMessage(topic, data, len, md);
    }
    BOOST_LOG_SEV(log, modmqttd::Log::info) << "PUBLISH " << topic << ": <" << v.val << ">";
    mPublishedTopics.insert(std::make_pair(topic, mPublishedTopics.size() + 1));
    mCondition.notify_all();
}

void
MockedMqttImpl::on_disconnect(int rc) {}

void
MockedMqttImpl::on_connect(int rc) {}

void
MockedMqttImpl::on_log(int level, const char* message) {}


bool
MockedMqttImpl::waitForPublish(const char* topic, std::chrono::milliseconds timeout) {
    BOOST_LOG_SEV(log, modmqttd::Log::info) << "Waiting for publish on: [" << topic << "]";
    std::unique_lock<std::mutex> lck(mMutex);
    bool published = mPublishedTopics.find(topic) != mPublishedTopics.end();
    if (!published) {
        auto start = std::chrono::steady_clock::now();
        int dur;
        do {
            if (mCondition.wait_for(lck, timeout) == std::cv_status::timeout)
                break;
            published = mPublishedTopics.find(topic) != mPublishedTopics.end();
            if (published)
                break;
            auto end = std::chrono::steady_clock::now();
            dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        } while (dur < timeout.count());
    }
    mPublishedTopics.erase(topic);
    return published;
}

std::string
MockedMqttImpl::waitForFirstPublish(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lck(mMutex);

    bool published = mPublishedTopics.size() != 0;
    if (!published) {
        auto start = std::chrono::steady_clock::now();
        int dur;
        do {
            if (mCondition.wait_for(lck, timeout) == std::cv_status::timeout)
                break;
            published = mPublishedTopics.size() != 0;
            if (published)
                break;
            auto end = std::chrono::steady_clock::now();
            dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        } while (dur < timeout.count());
    }

    if (!published)
        return std::string();

    int qval = 1;
    std::map<std::string, int>::const_iterator it = std::find_if(
        mPublishedTopics.begin(), mPublishedTopics.end(),
        [&qval](const std::pair<std::string, int>& item) -> bool { return qval == item.second; }
    );

    if (it == mPublishedTopics.end())
        throw MockedMqttException("Cannot find first published topic");

    std::string topic = it->first;
    mPublishedTopics.clear();
    BOOST_LOG_SEV(log, modmqttd::Log::info) << "Got first published topic: [" << topic << "]";
    return topic;
}


std::string
MockedMqttImpl::waitForMqttValue(const char* topic, const char* expected, std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lck(mMutex);
    BOOST_LOG_SEV(log, modmqttd::Log::info) << "Waiting for '" << expected << "' on: [" << topic << "]";
    std::string ret;
    auto start = std::chrono::steady_clock::now();
    int dur;
    int elapsed = 0;
    do {
        if (hasTopic(topic)) {
            ret = mqttValue(topic);
            break;
        }
        if (!waitForPublish(topic, timeout)) {
            break;
        }
        auto end = std::chrono::steady_clock::now();
        dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        elapsed += dur;
    } while (elapsed < timeout.count());
    mPublishedTopics.erase(topic);
    return ret;
}


bool
MockedMqttImpl::hasTopic(const char* topic) {
    std::unique_lock<std::mutex> lck(mMutex);
    return isTopicCreated(topic);
}

void
MockedMqttImpl::resetBroker() {
    BOOST_LOG_SEV(log, modmqttd::Log::info) << "MQTT Broker simulated restart";
    {
        //clear all values and subscriptions
        std::unique_lock<std::mutex> lck(mMutex);
        mTopics.clear();
        mPublishedTopics.clear();
        mSubscriptions.clear();
    }
    disconnect();
}

bool
MockedMqttImpl::isTopicCreated(const char* topic) const {
    std::map<std::string, MqttValue>::const_iterator it = mTopics.find(topic);
    if (it != mTopics.end())
        return true;
    return false;
}

std::string
MockedMqttImpl::mqttValue(const char* topic) {
    std::unique_lock<std::mutex> lck(mMutex);
    std::map<std::string, MqttValue>::const_iterator it = mTopics.find(topic);
    if (it == mTopics.end())
        throw MockedMqttException(std::string(topic) + " not found");
    const MqttValue data = it->second;
    std::string val(reinterpret_cast<const char*>(data.val), data.len);
    return val;
}

modmqttd::MqttPublishProps
MockedMqttImpl::mqttValueProps(const char* topic) {
    std::unique_lock<std::mutex> lck(mMutex);
    std::map<std::string, MqttValue>::const_iterator it = mTopics.find(topic);
    if (it == mTopics.end())
        throw MockedMqttException(std::string(topic) + " not found");
    const MqttValue data = it->second;
    return data.mProps;
}

