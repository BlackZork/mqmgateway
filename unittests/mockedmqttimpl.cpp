#include "mockedmqttimpl.hpp"

#include "libmodmqttsrv/mqttclient.hpp"
#include "libmodmqttsrv/threadutils.hpp"

#include "timing.hpp"

struct MsgEndThread {};

struct MsgPublishId {
    public:
        MsgPublishId(int id) : mId(id) {}
        int mId;
};

MockedMqttImpl::MockedMqttImpl() {
}

void
MockedMqttImpl::threadLoop(MockedMqttImpl& owner) {
    modmqttd::ThreadUtils::set_thread_name("mqttmock");
    modmqttd::QueueItem item;
    while (owner.mThreadQueue.wait_dequeue_timed(item, timing::maxTestTime)) {
        if (item.isSameAs(typeid(MsgEndThread)))
            return;

        if (item.isSameAs(typeid(MsgPublishId))) {
            auto msg = item.getData<MsgPublishId>();
            std::this_thread::sleep_for(std::chrono::milliseconds(3));
            owner.on_publish(msg->mId);
        }
    }
}

void
MockedMqttImpl::init(modmqttd::MqttClient* owner, const char* clientId) {
    mOwner = owner;
}

void
MockedMqttImpl::connect(const modmqttd::MqttBrokerConfig& config) {
    mConfig = config;
    stopThread();
    mThread.reset(new std::thread(threadLoop, std::ref(*this)));
    mOwner->onConnect();
}

void
MockedMqttImpl::reconnect() {
    connect(mConfig);
}

void
MockedMqttImpl::disconnect() {
    stopThread();
    mOwner->onDisconnect();
}

void
MockedMqttImpl::stopThread() {
    if (mThread != nullptr) {
        mThreadQueue.enqueue(modmqttd::QueueItem::create(MsgEndThread()));
        mThread->join();
        mThread.reset();
    }
}

void
MockedMqttImpl::stop() {
    stopThread();
}

void
MockedMqttImpl::subscribe(const char* topic) {
    std::unique_lock<std::mutex> lck(mMutex);
    mSubscriptions.insert(topic);
    spdlog::info("TEST: subscribe {}", topic);
    mCondition.notify_all();
}

int
MockedMqttImpl::publish(const char* topic, int len, const void* data, bool retain) {
    std::unique_lock<std::mutex> lck(mMutex);

    int publishCount = 0;
    auto it = mTopics.find(topic);
    if (it  != mTopics.end()) {
        publishCount = it->second.publishCount + 1;
    } else {
        publishCount = 1;
    }

    MqttValue v(data, len);
    v.publishCount = publishCount;
    mTopics[topic] = v;
    std::set<std::string>::const_iterator sit = mSubscriptions.find(topic);
    if (sit != mSubscriptions.end()) {
        mOwner->onMessage(topic, data, len);
    } else {
        mThreadQueue.enqueue(modmqttd::QueueItem::create(MsgPublishId(++mNextMessageId)));
    }
    spdlog::info("TEST: publish {}: <{}>", topic, v.val);


    mPublishedTopics.insert(std::make_pair(topic, mPublishedTopics.size() + 1));
    mCondition.notify_all();

    return mNextMessageId;
}

void
MockedMqttImpl::on_disconnect(int rc) {}

void
MockedMqttImpl::on_connect(int rc) {}

void
MockedMqttImpl::on_publish(int messageId) {
    mOwner->onPublish(messageId);
}

void
MockedMqttImpl::on_log(int level, const char* message) {}

bool
MockedMqttImpl::waitForPublish(const char* topic, std::chrono::milliseconds timeout) {
    spdlog::info("TEST: Waiting {} ms for publish on: [{}]", timeout, topic);
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

bool
MockedMqttImpl::waitForSubscription(const char* topic, std::chrono::milliseconds timeout) {
    spdlog::info("TEST: Waiting {} for subscription on: [{}]", timeout, topic);
    std::unique_lock<std::mutex> lck(mMutex);
    bool subscribed = mSubscriptions.find(topic) != mSubscriptions.end();
    if (!subscribed) {
        auto start = std::chrono::steady_clock::now();
        int dur;
        do {
            if (mCondition.wait_for(lck, timeout) == std::cv_status::timeout)
                break;
            subscribed = mSubscriptions.find(topic) != mSubscriptions.end();
            if (subscribed)
                break;
            auto end = std::chrono::steady_clock::now();
            dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        } while (dur < timeout.count());
    }
    return subscribed;
}


int
MockedMqttImpl::getPublishCount(const char* topic) {
    auto it = mTopics.find(topic);
    if (it == mTopics.end())
        return 0;
    return it->second.publishCount;
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
    mPublishedTopics.erase(it);
    spdlog::info("TEST: Got first published topic: [{}]", topic);
    return topic;
}


std::string
MockedMqttImpl::waitForMqttValue(const char* topic, const char* expected, std::chrono::milliseconds timeout) {
    spdlog::info("TEST: Waiting for '{}' on: [{}]", expected, topic);
    std::string ret;
    auto start = std::chrono::steady_clock::now();
    int dur;
    int elapsed = 0;
    do {
        if (!waitForPublish(topic, timeout)) {
            break;
        }
        if (hasTopic(topic)) {
            ret = mqttValue(topic);
            if (ret == expected)
                break;
            else
                ret.clear();
        }
        auto end = std::chrono::steady_clock::now();
        dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        elapsed += dur;
    } while (elapsed < timeout.count());

    return ret;
}


bool
MockedMqttImpl::hasTopic(const char* topic) {
    std::unique_lock<std::mutex> lck(mMutex);
    std::map<std::string, MqttValue>::const_iterator it = mTopics.find(topic);
    if (it != mTopics.end())
        return true;
    return false;
}

void
MockedMqttImpl::resetBroker() {
    spdlog::info("TEST: MQTT Broker simulated restart");
    {
        //clear all values and subscriptions
        std::unique_lock<std::mutex> lck(mMutex);
        mTopics.clear();
        mPublishedTopics.clear();
        mSubscriptions.clear();
    }
    disconnect();
}

std::string
MockedMqttImpl::mqttValue(const char* topic) {
    std::unique_lock<std::mutex> lck(mMutex);
    std::map<std::string, MqttValue>::const_iterator it = mTopics.find(topic);
    if (it == mTopics.end())
        throw MockedMqttException(std::string(topic) + " not found");
    const MqttValue data = it->second;
    std::string val(static_cast<const char*>(data.val), data.len);
    return val;
}

bool
MockedMqttImpl::mqttNullValue(const char* topic) {
    std::unique_lock<std::mutex> lck(mMutex);
    std::map<std::string, MqttValue>::const_iterator it = mTopics.find(topic);
    if (it == mTopics.end())
        throw MockedMqttException(std::string(topic) + " not found");
    const MqttValue data = it->second;
    return data.len == 0;
}

MockedMqttImpl::~MockedMqttImpl() {
    if (mThread != nullptr)
        stop();
}
