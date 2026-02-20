#pragma once

#include <thread>
#include <iostream>

#include "libmodmqttsrv/modmqtt.hpp"
#include "libmodmqttsrv/threadutils.hpp"

#include "mockedmqttimpl.hpp"
#include "mockedmodbuscontext.hpp"
#include "defaults.hpp"
#include "timing.hpp"

#include "catch2/catch_all.hpp"

class ModMqttServerThread {
    public:
        ModMqttServerThread(const std::string& config, bool check_init=true)
        : mConfig(config),
          mCheckInit(check_init)
        {};

        void RequireNoThrow() {
            if(mException.get() != nullptr) {
                CAPTURE(mException->what());
            }
            CHECK(mException.get() == nullptr);
        }

        template <typename T> void requireException(const std::string& messagePart) {
            CHECK(mException.get() != nullptr);
            const std::exception& ex(*mException);
            CHECK(typeid(T) == typeid(ex));
            CHECK_THAT(ex.what(), Catch::Matchers::ContainsSubstring(messagePart));
        }

        void start() {
            mInitError = false;
            mException.reset();
            mServerThread.reset(new std::thread(run_server, std::ref(mConfig), std::ref(*this)));
        }

        void stop() {
            if (mServerThread.get() == nullptr)
                return;
            if (!mInitError)
                mServer.stop();
            mServerThread->join();
            mServerThread.reset();
            if (mCheckInit)
                RequireNoThrow();
        }

        bool initOk() const {
            return !mInitError;
        }

        const std::exception& getException() const {
            CHECK(mException != nullptr);
            return *mException;
        }

        const modmqttd::ModMqtt& getServer() const { return mServer; }

        ~ModMqttServerThread() {
            stop();
        }
    protected:
        static void run_server(const std::string& config, ModMqttServerThread& master) {
            modmqttd::ThreadUtils::set_thread_name("test");
            try {
                YAML::Node cfg = YAML::Load(config);
                master.mServer.init(cfg, true);
                master.mServer.start();
            } catch (const modmqttd::ConfigurationException& ex) {
                std::cerr << "Bad config: " << ex.what() << std::endl;
                master.mException.reset(new modmqttd::ConfigurationException(ex));
                master.mInitError = true;
            } catch (const std::exception& ex) {
                std::cerr << ex.what() << std::endl;
                master.mException.reset(new std::exception(ex));
            }
        };
        std::string mConfig;
        modmqttd::ModMqtt mServer;
        std::shared_ptr<std::thread> mServerThread;
        std::shared_ptr<std::exception> mException;
        bool mInitError;
        bool mCheckInit;

};

class MockedModMqttServerThread : public ModMqttServerThread {
    public:
        MockedModMqttServerThread(const std::string& config, bool check_init=true) : ModMqttServerThread(config, check_init) {
            mMqtt.reset(new MockedMqttImpl());
            mModbusFactory.reset(new MockedModbusFactory());

            mServer.setMqttImplementation(mMqtt);
            mServer.setModbusContextFactory(mModbusFactory);
            //allow to find converter dlls when cwd is in binary directory
            mServer.addConverterPath("../stdconv");
            mServer.addConverterPath("../exprconv");
        };

    void waitForSubscription(const char* topic, std::chrono::milliseconds timeout = timing::defaultWait) {
        INFO("Checking for subscription on " << topic);
        bool is_subscribed = mMqtt->waitForSubscription(topic, timeout);
        CAPTURE(topic);
        REQUIRE(is_subscribed == true);
    }

    /**
     * There is no signal after initial poll is done
     * This function will figure it out by checking that all registers were read at least once
     * Warning: it will not work if you setModbusRegisterValue() for register
     * that is not in poll specification (not used in any mqtt object config)
     */
    void waitForInitialPoll(const char* pNetworkName, std::chrono::milliseconds timeout = timing::defaultWait) {
        mModbusFactory->getMockedModbusContext(pNetworkName).waitForInitialPoll(timeout);
    }

    void waitForPublish(const char* topic, std::chrono::milliseconds timeout = timing::defaultWait) {
        INFO("Checking for publish on " << topic);
        bool is_published = mMqtt->waitForPublish(topic, timeout);
        CAPTURE(topic);
        REQUIRE(is_published == true);
    }

    void requirePublishCount(const char* topic, int count) {
        CAPTURE(topic);
        REQUIRE(mMqtt->getPublishCount(topic) == count);
    }

    int getPublishCount(const char* topic) {
        return mMqtt->getPublishCount(topic);
    }

    std::string waitForFirstPublish(std::chrono::milliseconds timeout = timing::defaultWait) {
        std::string topic = mMqtt->waitForFirstPublish(timeout);
        INFO("Getting first published topic");
        REQUIRE(!topic.empty());
        return topic;
    }

    void publish(const char* topic, const std::string& value, bool retain = false) {
        mMqtt->publish(topic, value.length(), value.c_str(), retain);
    }

    void waitForMqttValue(const char* topic, const char* expected, std::chrono::milliseconds timeout = timing::defaultWait) {
        std::string current  = mMqtt->waitForMqttValue(topic, expected, timeout);
        REQUIRE(current == expected);
    }

    std::string mqttValue(const char* topic) {
        bool has_topic = mMqtt->hasTopic(topic);
        REQUIRE(has_topic);
        return mMqtt->mqttValue(topic);
    }

    bool mqttNullValue(const char* topic) {
        bool has_topic = mMqtt->hasTopic(topic);
        REQUIRE(has_topic);
        return mMqtt->mqttNullValue(topic);
    }

    void setModbusRegisterValue(const char* network, int slaveId, int regNum, modmqttd::RegisterType regtype, uint16_t val) {
        mModbusFactory->setModbusRegisterValue(network, slaveId, regNum, regtype, val);
    }

    uint16_t getModbusRegisterValue(const char* network, int slaveId, int regNum, modmqttd::RegisterType regtype) {
        return mModbusFactory->getModbusRegisterValue(network, slaveId, regNum, regtype);
    }

    void waitForModbusValue(const char* network, int slaveId, int regNum, modmqttd::RegisterType regType, uint16_t val, std::chrono::milliseconds timeout = timing::defaultWait) {
        uint16_t current_val = mModbusFactory->waitForModbusValue(network, slaveId, regNum, regType, val, timeout);
        REQUIRE(current_val == val);
    }

    std::chrono::time_point<std::chrono::steady_clock>
    getLastPollTime(const char* network = nullptr) const {
        return mModbusFactory->getLastPollTime(network);
    }

    void disconnectModbusSlave(const char* network, int slaveId) {
        mModbusFactory->disconnectModbusSlave(network, slaveId);
    }

    void connectModbusSlave(const char* network, int slaveId) {
        mModbusFactory->connectModbusSlave(network, slaveId);
    }

    void disconnectSerialPortFor(const char* networkName) {
        mModbusFactory->getMockedModbusContext(networkName).removeFakeDevice();
    }


    void setModbusRegisterReadError(const char* network, int slaveId, int regNum, modmqttd::RegisterType regtype) {
        mModbusFactory->setModbusRegisterReadError(network, slaveId, regNum, regtype);
    }

    void clearModbusRegisterReadError(const char* network, int slaveId, int regNum, modmqttd::RegisterType regtype) {
        mModbusFactory->clearModbusRegisterReadError(network, slaveId, regNum, regtype);
    }

    MockedModbusContext& getMockedModbusContext(const std::string& networkName) const {
        return mModbusFactory->getMockedModbusContext(networkName);
    }

    std::shared_ptr<MockedModbusFactory> mModbusFactory;
    std::shared_ptr<MockedMqttImpl> mMqtt;
};
