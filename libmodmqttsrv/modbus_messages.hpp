#pragma once

#include <string>
#include <map>
#include <chrono>
#include <vector>

#include "logging.hpp"
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

class MsgRegisterMessageBase {
    public:
        MsgRegisterMessageBase(int slaveId, RegisterType regType, int registerNumber, int registerCount)
            : mSlaveId(slaveId), mRegisterType(regType), mRegisterNumber(registerNumber), mCount(registerCount) {}
        int mSlaveId;
        RegisterType mRegisterType;
        int mRegisterNumber;
        int mCount;
};

class MsgRegisterValues : public MsgRegisterMessageBase {
    public:
        MsgRegisterValues(int slaveId, RegisterType regType, int registerNumber, const ModbusRegisters& registers)
            : MsgRegisterMessageBase(slaveId, regType, registerNumber, registers.getCount()),
              mRegisters(registers) {}
        MsgRegisterValues(int slaveId, RegisterType regType, int registerNumber, const std::vector<uint16_t>& registers)
            : MsgRegisterMessageBase(slaveId, regType, registerNumber, registers.size()),
              mRegisters(registers) {}

        ModbusRegisters mRegisters;
};

class MsgRegisterReadFailed : public MsgRegisterMessageBase {
    public:
        MsgRegisterReadFailed(int slaveId, RegisterType regType, int registerNumber, int registerCount)
            : MsgRegisterMessageBase(slaveId, regType, registerNumber, registerCount)
        {}
};

class MsgRegisterWriteFailed : public MsgRegisterMessageBase {
    public:
        MsgRegisterWriteFailed(int slaveId, RegisterType regType, int registerNumber, int registerCount)
            : MsgRegisterMessageBase(slaveId, regType, registerNumber, registerCount)
        {}
};


class MsgRegisterPoll {
    public:
        static constexpr int INVALID_REFRESH = -1;
        MsgRegisterPoll(int registerNumber, int count=1)
            : mRegister(registerNumber), mCount(count)
        {
#ifndef NDEBUG
            if (mRegister < 0)
                throw DebugException("Invalid register number");
            if (mCount <= 0)
                throw DebugException("Count cannot be 0 or negative");
#endif
        }

        boost::log::sources::severity_logger<Log::severity> log;

        int mSlaveId;
        int mRegister;
        int mCount;
        RegisterType mRegisterType;
        int mRefreshMsec = INVALID_REFRESH;

        void merge(const MsgRegisterPoll& other);
        bool overlaps(const MsgRegisterPoll& poll) const;
        bool isConsecutiveOf(const MsgRegisterPoll& other) const;
        bool isSameAs(const MsgRegisterPoll& other) const;
        int firstRegister() const { return mRegister; }
        int lastRegister() const { return (mRegister + mCount) - 1; }
};

class MsgRegisterPollSpecification {
    public:
        boost::log::sources::severity_logger<Log::severity> log;

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
