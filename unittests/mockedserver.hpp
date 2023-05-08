#pragma once

#include <thread>

#include "catch2/catch.hpp"
#include "libmodmqttsrv/modmqtt.hpp"

#include "mockedmqttimpl.hpp"
#include "mockedmodbuscontext.hpp"


#include <iostream>

class ModMqttServerThread {
    public:
        ModMqttServerThread(const std::string& config) : mConfig(config) {};

        void RequireNoThrow() {
            CHECK(mException == std::string());
        }

        void start() {
            mInitError = false;
            mServerThread.reset(new std::thread(run_server, std::ref(mConfig), std::ref(*this)));
        }

        void stop() {
            if (mServerThread.get() == nullptr)
                return;
            if (!mInitError)
                mServer.stop();
            mServerThread->join();
            mServerThread.reset();
            RequireNoThrow();
        }

        ~ModMqttServerThread() {
            stop();
        }
    protected:
        static void run_server(const std::string& config, ModMqttServerThread& master) {
            try {
                YAML::Node cfg = YAML::Load(config);
                master.mServer.init(cfg);
                master.mServer.start();
            } catch (const modmqttd::ConfigurationException& ex) {
                std::cerr << "Bad config: " << ex.what() << std::endl;
                master.mException = ex.what();
                master.mInitError = true;
            } catch (const std::exception& ex) {
                std::cerr << ex.what() << std::endl;
                master.mException = ex.what();
            }
        };
        std::string mConfig;
        modmqttd::ModMqtt mServer;
        std::shared_ptr<std::thread> mServerThread;
        std::string mException;
        bool mInitError;

};

class MockedModMqttServerThread : public ModMqttServerThread {
    public:
        MockedModMqttServerThread(const std::string& config) : ModMqttServerThread(config) {
            mMqtt.reset(new MockedMqttImpl());
            mModbusFactory.reset(new MockedModbusFactory());

            mServer.setMqttImplementation(mMqtt);
            mServer.setModbusContextFactory(mModbusFactory);
            //allow to find converter dlls when cwd is in binary directory
            mServer.addConverterPath("../stdconv");
            mServer.addConverterPath("../exprconv");
        };

    void waitForPublish(const char* topic, std::chrono::milliseconds timeout = std::chrono::milliseconds(100)) {
        INFO("Checking for publish on " << topic);
        bool is_published = mMqtt->waitForPublish(topic, timeout);
        REQUIRE(is_published == true);
    }

    bool checkForPublish(const char* topic, std::chrono::milliseconds timeout = std::chrono::milliseconds(100)) {
        INFO("Checking for publish on " << topic);
        return mMqtt->waitForPublish(topic, timeout);
    }

    std::string waitForFirstPublish(std::chrono::milliseconds timeout = std::chrono::milliseconds(100)) {
        std::string topic = mMqtt->waitForFirstPublish(timeout);
        INFO("Getting first published topic");
        REQUIRE(!topic.empty());
        return topic;
    }

    void publish(const char* topic, const std::string& value) {
        mMqtt->publish(topic, value.length(), value.c_str());
    }

    void publish(const char* topic, const std::string& value, const modmqttd::MqttPublishProps& props) {
        mMqtt->publish(topic, value.length(), value.c_str(), props);
    }

    void waitForMqttValue(const char* topic, const char* expected, std::chrono::milliseconds timeout = std::chrono::milliseconds(100)) {
        std::string current  = mMqtt->waitForMqttValue(topic, expected, timeout);
        REQUIRE(current == expected);
    }

    std::string mqttValue(const char* topic) {
        bool has_topic = mMqtt->hasTopic(topic);
        REQUIRE(has_topic);
        return mMqtt->mqttValue(topic);
    }

    modmqttd::MqttPublishProps mqttValueProps(const char* topic) {
        bool has_topic = mMqtt->hasTopic(topic);
        REQUIRE(has_topic);
        return mMqtt->mqttValueProps(topic);
    }

    void setModbusRegisterValue(const char* network, int slaveId, int regNum, modmqttd::RegisterType regtype, uint16_t val) {
        mModbusFactory->setModbusRegisterValue(network, slaveId, regNum, regtype, val);
    }

    uint16_t getModbusRegisterValue(const char* network, int slaveId, int regNum, modmqttd::RegisterType regtype) {
        return mModbusFactory->getModbusRegisterValue(network, slaveId, regNum, regtype);
    }

    int getModbusRegisterReadCount(const char* network, int slaveId, int regNum, modmqttd::RegisterType regType) {
        return mModbusFactory->getRegisterReadCount(network, slaveId, regNum, regType);
    }

    void disconnectModbusSlave(const char* network, int slaveId) {
        mModbusFactory->disconnectModbusSlave(network, slaveId);
    }

    void setModbusRegisterError(const char* network, int slaveId, int regNum, modmqttd::RegisterType regtype) {
        mModbusFactory->setModbusRegisterError(network, slaveId, regNum, regtype);
    }

    std::shared_ptr<MockedModbusFactory> mModbusFactory;
    std::shared_ptr<MockedMqttImpl> mMqtt;
};
