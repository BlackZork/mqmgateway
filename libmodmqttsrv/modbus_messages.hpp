#pragma once

#include <string>
#include <map>
#include <chrono>
#include <vector>

#include "modbus_types.hpp"
#include "libmodmqttconv/modbusregisters.hpp"
#include "debugtools.hpp"

namespace modmqttd {

class MsgMqttCommand {
    public:
        int mSlave;
        int mRegister;
        RegisterType mRegisterType;
        int16_t mData;
};

class MsgRegisterMessageBase : public ModbusAddressRange {
    public:
        MsgRegisterMessageBase(int pSlaveId, int pRegisterNumber, RegisterType pType, int pCount)
            : ModbusAddressRange(pRegisterNumber, pType, pCount),
              mSlaveId(pSlaveId)
        {}

        int mSlaveId;
};

class MsgRegisterValues : public MsgRegisterMessageBase {
    public:
        MsgRegisterValues(int slaveId, RegisterType regType, int registerNumber, const ModbusRegisters& registers)
            : MsgRegisterMessageBase(slaveId, registerNumber, regType, registers.getCount()),
              mRegisters(registers),
              mCreationTime(std::chrono::steady_clock::now())
            {}
        MsgRegisterValues(int slaveId, RegisterType regType, int registerNumber, const std::vector<uint16_t>& registers)
            : MsgRegisterMessageBase(slaveId, registerNumber, regType, registers.size()),
              mRegisters(registers), 
              mCreationTime(std::chrono::steady_clock::now())
            {}

        std::chrono::steady_clock::time_point mCreationTime;
        ModbusRegisters mRegisters;
};

class MsgRegisterReadFailed : public MsgRegisterMessageBase {
    public:
        MsgRegisterReadFailed(int slaveId, RegisterType regType, int registerNumber, int registerCount)
            : MsgRegisterMessageBase(slaveId, registerNumber, regType, registerCount)
        {}
};

class MsgRegisterWriteFailed : public MsgRegisterMessageBase {
    public:
        MsgRegisterWriteFailed(int slaveId, RegisterType regType, int registerNumber, int registerCount)
            : MsgRegisterMessageBase(slaveId, registerNumber, regType, registerCount)
        {}
};


class MsgRegisterPoll : public MsgRegisterMessageBase {
    public:
        static constexpr std::chrono::milliseconds INVALID_REFRESH = std::chrono::milliseconds(-1);
        MsgRegisterPoll(int pSlaveId, int pRegisterNumber, RegisterType pType, int pCount=1)
            : MsgRegisterMessageBase(pSlaveId, pRegisterNumber, pType, pCount)
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
};

class MsgRegisterPollSpecification {
    public:
        static boost::log::sources::severity_logger<Log::severity> log;

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
