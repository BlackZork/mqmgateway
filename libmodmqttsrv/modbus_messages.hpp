#pragma once

#include <string>
#include <map>
#include <chrono>
#include <vector>

#include "libmodmqttconv/modbusregisters.hpp"

#include "common.hpp"
#include "modbus_types.hpp"
#include "debugtools.hpp"

namespace modmqttd {

class MsgRegisterValues : public ModbusRequestBase {
    public:
        MsgRegisterValues(int slaveId, RegisterType regType, int registerNumber, const ModbusRegisters& registers, int pCommandId, ModbusWriteMode pWriteMode)
            : ModbusRequestBase(slaveId, registerNumber, regType, registers.getCount(), pCommandId),
              mRegisters(registers),
              mCreationTime(std::chrono::steady_clock::now()),
              mWriteMode(pWriteMode) {}
        MsgRegisterValues(int slaveId, RegisterType regType, int registerNumber, const std::vector<uint16_t>& registers, int pCommandId = 0)
            : ModbusRequestBase(slaveId, registerNumber, regType, registers.size(), pCommandId),
              mRegisters(registers),
              mCreationTime(std::chrono::steady_clock::now()) {}

        const std::chrono::steady_clock::time_point& getCreationTime() const { return mCreationTime; }

        ModbusRegisters mRegisters;
        ModbusWriteMode mWriteMode = ModbusWriteMode::AUTO;

    private:
        std::chrono::steady_clock::time_point mCreationTime;
};

class MsgRegisterReadFailed : public ModbusRequestBase {
    public:
        MsgRegisterReadFailed(int slaveId, RegisterType regType, int registerNumber, int registerCount, int pCommandId = 0)
            : ModbusRequestBase(slaveId, registerNumber, regType, registerCount, pCommandId) {}
};

class MsgRegisterWriteFailed : public ModbusRequestBase {
    public:
        MsgRegisterWriteFailed(int slaveId, RegisterType regType, int registerNumber, int registerCount, int pCommandId = 0)
            : ModbusRequestBase(slaveId, registerNumber, regType, registerCount, pCommandId) {}
};

class MsgRegisterReadRequest : public ModbusRequestBase {
    public:
        MsgRegisterReadRequest(int slaveId, RegisterType regType, int registerNumber, int registerCount, int pCommandId)
            : ModbusRequestBase(slaveId, registerNumber, regType, registerCount, pCommandId) {}
};


class MsgRegisterPoll : public ModbusRequestBase {
    public:
        static constexpr std::chrono::milliseconds INVALID_REFRESH = std::chrono::milliseconds(-1);
        MsgRegisterPoll(int pSlaveId, int pRegisterNumber, RegisterType pType, int pCount = 1)
            : ModbusRequestBase(pSlaveId, pRegisterNumber, pType, pCount) {
#ifndef NDEBUG
            if (mRegister < 0)
                throw DebugException("Invalid register number");
            if (mCount <= 0)
                throw DebugException("Count cannot be 0 or negative");
#endif
        }

        bool isSameAs(const MsgRegisterPoll& other) const;
        void merge(const MsgRegisterPoll& other);

        std::chrono::milliseconds mRefreshMsec = INVALID_REFRESH;
        PublishMode mPublishMode = PublishMode::ON_CHANGE;
};

class MsgRegisterPollSpecification {
    public:
        MsgRegisterPollSpecification(const std::string& networkName) : mNetworkName(networkName) {}

        /*!
            Convert subsequent slave registers
            of the same type to single MsgRegisterPoll instance
            with corresponding mCount value

            This method will not join overlapping groups.
        */
        void group();

        void merge(const std::vector<MsgRegisterPoll>& lst) {
            for (auto& poll: lst)
                merge(poll);
        }

        /*!
            Merge overlapping
            register group with arg and adjust refresh time
            or add new register to poll

        */
        void merge(const MsgRegisterPoll& poll);

        std::string mNetworkName;
        std::vector<MsgRegisterPoll> mRegisters;
};

class MsgModbusNetworkState {
    public:
        MsgModbusNetworkState(const std::string& networkName, bool isUp)
            : mNetworkName(networkName), mIsUp(isUp) {}
        bool mIsUp;
        std::string mNetworkName;
};

class MsgMqttNetworkState {
    public:
        MsgMqttNetworkState(bool isUp)
            : mIsUp(isUp) {}
        bool mIsUp;
};

class EndWorkMessage {
        // no fields here, thread will check type of message and exit
};

}
