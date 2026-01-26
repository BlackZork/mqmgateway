#pragma once

#include <map>
#include <chrono>
#include <vector>
#include <mutex>
#include <tuple>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <cstdio>

#include "libmodmqttsrv/imodbuscontext.hpp"
#include "libmodmqttsrv/modbus_types.hpp"
#include "libmodmqttsrv/logging.hpp"
#include "libmodmqttsrv/config.hpp"

class MockedModbusContext : public modmqttd::IModbusContext {
    public:
        static const std::chrono::milliseconds sDefaultSlaveReadTime;
        static const std::chrono::milliseconds sDefaultSlaveWriteTime;

        struct RegData {
            uint16_t mValue = 0;
            bool mError = false;
            int mReadCount = 0;
            int mWriteCount = 0;
        };

        class Slave {
            public:
                Slave(const std::shared_ptr<std::condition_variable>& ioCondition, int id = 0) : mIOCondition(ioCondition), mId(id) {}
                void write(const modmqttd::RegisterWrite& msg, bool internalOperation = false);
                std::vector<uint16_t> read(const modmqttd::RegisterPoll& regData, bool internalOperation = false);

                void setDisconnected(bool flag = true) { mDisconnected = flag; }
                void setError(int regNum, modmqttd::RegisterType regType, bool flag = true);
                void clearError(int regNum, modmqttd::RegisterType regType)
                    { setError(regNum, regType, false); }
                bool hasError(int regNum, modmqttd::RegisterType regType, int regCount) const;
                int getReadCount() const { return mReadCount; }
                int getWriteCount() const { return mWriteCount; }

                std::map<int, RegData> mInput;
                std::map<int, RegData> mHolding;
                std::map<int, RegData> mCoil;
                std::map<int, RegData> mBit;

                std::chrono::milliseconds mReadTime = sDefaultSlaveReadTime;
                std::chrono::milliseconds mWriteTime = sDefaultSlaveWriteTime;
                int mId;

            private:
                bool hasError(const std::map<int, MockedModbusContext::RegData>& table, int num, int count) const;
                std::vector<uint16_t> readRegisters(std::map<int, RegData>& table, int num, int count, bool internalOperation);
                uint16_t readRegister(std::map<int, RegData>& table, int num, bool internalOperation);
                bool mDisconnected = false;
                int mReadCount = 0;
                int mWriteCount = 0;
                std::shared_ptr<std::condition_variable> mIOCondition;
        };

        MockedModbusContext() {
            mCondition.reset(new std::condition_variable());
        }

        virtual void init(const modmqttd::ModbusNetworkConfig& config);
        virtual void connect();
        virtual bool isConnected() const;
        virtual void disconnect();

        virtual std::vector<uint16_t> readModbusRegisters(int slaveId, const modmqttd::RegisterPoll& regData);
        virtual void writeModbusRegisters(int slaveId, const modmqttd::RegisterWrite& msg);
        virtual modmqttd::ModbusNetworkConfig::Type getNetworkType() const { return modmqttd::ModbusNetworkConfig::Type::TCPIP; };
        virtual uint16_t waitForModbusValue(int slaveId, int regNum, modmqttd::RegisterType regType, uint16_t val, std::chrono::milliseconds timeout);
        virtual uint16_t getModbusRegisterValue(int slaveId, int regNum, modmqttd::RegisterType regtype);
        virtual void waitForInitialPoll(std::chrono::milliseconds timeout);


        int getReadCount(int slaveId) const;
        int getWriteCount(int slaveId) const;
        int getConnectionCount() const { return mConnectionCount; }

        void removeFakeDevice() { std::remove(mDeviceName.c_str()); }

        void createFakeDevice();

        std::tuple<int,int> getLastReadRegisterAddress() const {
            return std::tuple<int,int>(mLastPolledSlave, mLastPolledRegister+1);
        }
        const std::chrono::time_point<std::chrono::steady_clock>& getLastPollTime() const {
            return mLastPollTime;
        }

        Slave& getSlave(int slaveId);

        bool mInternalOperation = false;
        std::string mNetworkName;
        std::string mDeviceName;
        std::fstream mDeviceFile;

        ~MockedModbusContext() {
            mDeviceFile.close();
            std::remove(mDeviceName.c_str());
        }
    private:
        std::mutex mMutex;
        std::shared_ptr<std::condition_variable> mCondition;
        std::map<int, Slave> mSlaves;

        std::chrono::time_point<std::chrono::steady_clock> mLastPollTime;
        int mLastPolledSlave;
        int mLastPolledRegister;
        int mConnectionCount = 0;

        std::map<int, MockedModbusContext::Slave>::iterator findOrCreateSlave(int id);
        int getUnreadedRegisterCount();
        int getUnreadedRegisterCount(const std::map<int, RegData>& pRegData);
};

class MockedModbusFactory : public modmqttd::IModbusFactory {
    public:
        virtual std::shared_ptr<modmqttd::IModbusContext> getContext(const std::string& networkName) {
            return getOrCreateContext(networkName.c_str());
        };

        std::chrono::time_point<std::chrono::steady_clock> getLastPollTime(const char* network = nullptr) const;
        std::tuple<int, int> getLastReadRegisterAddress(const char* network = nullptr) const;

        void setModbusRegisterValue(const char* network, int slaveId, int regNum, modmqttd::RegisterType regtype, uint16_t val);
        uint16_t getModbusRegisterValue(const char* network, int slaveId, int regNum, modmqttd::RegisterType regtype);
        void setModbusRegisterReadError(const char* network, int slaveId, int regNum, modmqttd::RegisterType regtype, bool pFlag = true);
        void clearModbusRegisterReadError(const char* network, int slaveId, int regNum, modmqttd::RegisterType regtype) {
            setModbusRegisterReadError(network, slaveId, regNum, regtype, false);
        }

        void setModbusRegisterWriteError(const char* network, int slaveId, int regNum, modmqttd::RegisterType regtype, bool pFlag = true);
        void clearModbusRegisterWriteError(const char* network, int slaveId, int regNum, modmqttd::RegisterType regtype) {
            setModbusRegisterWriteError(network, slaveId, regNum, regtype, false);
        }

        MockedModbusContext& getMockedModbusContext(const std::string& networkName) const {
            auto it = mModbusNetworks.find(networkName);
            return *(it->second);
        }
        void disconnectModbusSlave(const char* network, int slaveId);
        void connectModbusSlave(const char* network, int slaveId);

        void disconnectSerialPortFor(const char* network);

        uint16_t waitForModbusValue(const char* network, int slaveId, int regNum, modmqttd::RegisterType regType, uint16_t val, std::chrono::milliseconds timeout);
    private:
        std::shared_ptr<MockedModbusContext> getOrCreateContext(const char* network);
        std::shared_ptr<MockedModbusContext> findOrReturnFirstContext(const char* network) const;
        std::map<std::string, std::shared_ptr<MockedModbusContext>> mModbusNetworks;
};

