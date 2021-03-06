#pragma once

#include <string>
#include <map>
#include <chrono>
#include <vector>

#include "modbus_types.hpp"

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
        MsgRegisterMessageBase(int slaveId, RegisterType regType, int registerNumber)
            : mSlaveId(slaveId), mRegisterType(regType), mRegisterNumber(registerNumber) {}
        int mSlaveId;
        RegisterType mRegisterType;
        int mRegisterNumber;
};

class MsgRegisterValue : public MsgRegisterMessageBase {
    public:
        MsgRegisterValue(int slaveId, RegisterType regType, int registerNumber, int16_t value)
            : MsgRegisterMessageBase(slaveId, regType, registerNumber),
              mValue(value) {}
        int16_t mValue;
};

class MsgRegisterReadFailed : public MsgRegisterMessageBase {
    public:
        MsgRegisterReadFailed(int slaveId, RegisterType regType, int registerNumber)
            : MsgRegisterMessageBase(slaveId, regType, registerNumber)
        {}
};

class MsgRegisterWriteFailed : public MsgRegisterMessageBase {
    public:
        MsgRegisterWriteFailed(int slaveId, RegisterType regType, int registerNumber)
            : MsgRegisterMessageBase(slaveId, regType, registerNumber)
        {}
};


class MsgRegisterPoll {
    public:
        int mSlaveId;
        int mRegister;
        RegisterType mRegisterType;
        int mRefreshMsec;
};

class MsgRegisterPollSpecification {
    public:
        MsgRegisterPollSpecification(const std::string& networkName) : mNetworkName(networkName) {}
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
