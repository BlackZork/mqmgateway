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

class MsgRegisterValues : public ModbusSlaveAddressRange {
    public:
        MsgRegisterValues(int slaveId, RegisterType regType, int registerNumber, const ModbusRegisters& registers, int pCommandId)
            : ModbusSlaveAddressRange(slaveId, registerNumber, regType, registers.getCount()),
              mRegisters(registers),
              mCreationTime(std::chrono::steady_clock::now()),
              mCommandId(pCommandId)
            {}
        MsgRegisterValues(int slaveId, RegisterType regType, int registerNumber, const std::vector<uint16_t>& registers)
            : ModbusSlaveAddressRange(slaveId, registerNumber, regType, registers.size()),
              mRegisters(registers),
              mCreationTime(std::chrono::steady_clock::now())
            {}

        const std::chrono::steady_clock::time_point& getCreationTime() const { return mCreationTime; }
        int getCommandId() const { return mCommandId; }
        bool hasCommandId() const { return mCommandId != 0; }

        ModbusRegisters mRegisters;
    private:
        std::chrono::steady_clock::time_point mCreationTime;
        int mCommandId = 0;
};

class MsgRegisterReadFailed : public ModbusSlaveAddressRange {
    public:
        MsgRegisterReadFailed(int slaveId, RegisterType regType, int registerNumber, int registerCount)
            : ModbusSlaveAddressRange(slaveId, registerNumber, regType, registerCount)
        {}
};

class MsgRegisterWriteFailed : public ModbusSlaveAddressRange {
    public:
        MsgRegisterWriteFailed(int slaveId, RegisterType regType, int registerNumber, int registerCount)
            : ModbusSlaveAddressRange(slaveId, registerNumber, regType, registerCount)
        {}
};


class MsgRegisterPoll : public ModbusSlaveAddressRange {
    public:
        static constexpr std::chrono::milliseconds INVALID_REFRESH = std::chrono::milliseconds(-1);
        MsgRegisterPoll(int pSlaveId, int pRegisterNumber, RegisterType pType, int pCount=1)
            : ModbusSlaveAddressRange(pSlaveId, pRegisterNumber, pType, pCount)
        {
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
            for(auto& poll: lst)
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
            : mNetworkName(networkName), mIsUp(isUp)
        {}
        bool mIsUp;
        std::string mNetworkName;
};

class MsgMqttNetworkState {
    public:
        MsgMqttNetworkState(bool isUp)
            : mIsUp(isUp)
        {}
        bool mIsUp;
};

class EndWorkMessage {
    // no fields here, thread will check type of message and exit
};

}
