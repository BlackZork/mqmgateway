#pragma once

#include <map>
#include <chrono>
#include <vector>
#include <mutex>

#include "libmodmqttsrv/imodbuscontext.hpp"
#include "libmodmqttsrv/modbus_messages.hpp"
#include "libmodmqttsrv/modbus_types.hpp"
#include "libmodmqttsrv/logging.hpp"

class MockedModbusContext : public modmqttd::IModbusContext {
    public:
        static const std::chrono::milliseconds sDefaultSlaveReadTime;
        static const std::chrono::milliseconds sDefaultSlaveWriteTime;

        class Slave {
            struct RegData {
                uint16_t mValue;
                bool mError;
                int mReadCount;
            };
            public:
                Slave(int id = 0) : mId(id) {}
                void write(int registerAddress, modmqttd::RegisterType registerType, uint16_t value, bool internalOperation = false);
                uint16_t read(int registerAddress, modmqttd::RegisterType registerType, bool internalOperation = false);

                int getReadCount(int regNum, modmqttd::RegisterType registerType);

                void setDisconnected(bool flag = true) { mDisconnected = flag; }
                void setError(int regNum, modmqttd::RegisterType regType, bool flag = true);
                void clearError(int regNum, modmqttd::RegisterType regType)
                    { setError(regNum, regType, false); }
                bool hasError(int regNum, modmqttd::RegisterType regType) const;

                std::map<int, RegData> mInput;
                std::map<int, RegData> mHolding;
                std::map<int, RegData> mCoil;
                std::map<int, RegData> mBit;

                std::chrono::milliseconds mReadTime = sDefaultSlaveReadTime;
                std::chrono::milliseconds mWriteTime = sDefaultSlaveWriteTime;
                int mId;

            private:
                bool hasError(const std::map<int, MockedModbusContext::Slave::RegData>& table, int num) const;
                uint16_t readRegister(std::map<int, RegData>& table, int num, bool internalOperation);
                bool mDisconnected = false;
        };

        virtual void init(const modmqttd::ModbusNetworkConfig& config);
        virtual void connect() { mIsConnected = true; }
        virtual bool isConnected() const { return mIsConnected; }
        virtual void disconnect() { mIsConnected = false; }
        virtual uint16_t readModbusRegister(int slaveId, const modmqttd::RegisterPoll& regData);
        virtual std::vector<uint16_t> readModbusRegisters(const modmqttd::MsgRegisterReadRemoteCall& msg);
        virtual void writeModbusRegister(const modmqttd::MsgRegisterValue& msg);
        virtual void writeModbusRegisters(const modmqttd::MsgRegisterWriteRemoteCall& msg);

        Slave& getSlave(int slaveId);
        int getRegisterReadCount(int slaveId, int registerAddress, modmqttd::RegisterType registerType);

        bool mIsConnected = false;
        bool mInternalOperation = false;
        std::string mNetworkName;
    private:
        boost::log::sources::severity_logger<modmqttd::Log::severity> log;
        std::mutex mMutex;
        std::map<int, Slave> mSlaves;

        std::map<int, MockedModbusContext::Slave>::iterator findOrCreateSlave(int id);
};

class MockedModbusFactory : public modmqttd::IModbusFactory {
    public:
        virtual std::shared_ptr<modmqttd::IModbusContext> getContext(const std::string& networkName) {
            auto it = mModbusNetworks.find(networkName);
            std::shared_ptr<MockedModbusContext> ctx;
            if (it == mModbusNetworks.end()) {
                //it will be initialized by modbus_thread.cpp
                ctx.reset(new MockedModbusContext());
                mModbusNetworks[networkName] = ctx;
            } else {
                ctx = it->second;
            }
            return ctx;
        };

        uint16_t getModbusRegisterValue(const char* network, int slaveId, int regNum, modmqttd::RegisterType regtype);
        void setModbusRegisterValue(const char* network, int slaveId, int regNum, modmqttd::RegisterType regtype, uint16_t val);
        void setModbusRegisterError(const char* network, int slaveId, int regNum, modmqttd::RegisterType regtype);
        int getRegisterReadCount(const char* network, int slaveId, int regNum, modmqttd::RegisterType regtype);
        void disconnectModbusSlave(const char* network, int slaveId);
    private:
        std::shared_ptr<MockedModbusContext> getOrCreateContext(const char* network);
        std::map<std::string, std::shared_ptr<MockedModbusContext>> mModbusNetworks;
};

