#pragma once

#include <string>
#include <map>
#include <chrono>
#include <vector>

#include "modbus_types.hpp"

namespace modmqttd {

/**
 * Optional properties for MQTT publications
 * */
class MqttPublishProps {
    public:
        // Used in MQTT request-response
        std::string mResponseTopic;
        std::vector<uint8_t> mCorrelationData;
};

class MsgMqttCommand {
    public:
        int mSlave;
        int mRegister;
        RegisterType mRegisterType;
        int16_t mData;
};

class MsgRegisterMessageBase {
    public:
        MsgRegisterMessageBase(int slaveId, RegisterType regType, int registerAddress)
            : mSlaveId(slaveId), mRegisterType(regType), mRegisterAddress(registerAddress) {}
        int mSlaveId;
        RegisterType mRegisterType;
        int mRegisterAddress;
};

class MsgRegisterValue : public MsgRegisterMessageBase {
    public:
        MsgRegisterValue(int slaveId, RegisterType regType, int registerAddress, int16_t value)
            : MsgRegisterMessageBase(slaveId, regType, registerAddress),
              mValue(value) {}
        int16_t mValue;
};

class MsgRegisterReadRemoteCall : public MsgRegisterMessageBase {
    public:
        MsgRegisterReadRemoteCall(int slaveId, RegisterType regType, int registerAddress, int size, const MqttPublishProps& responseProps)
            : MsgRegisterMessageBase(slaveId, regType, registerAddress), mSize(size), mResponseProps(responseProps) { }
        int mSize;
        MqttPublishProps mResponseProps;
};

class MsgRegisterWriteRemoteCall : public MsgRegisterMessageBase {
    public:
        MsgRegisterWriteRemoteCall(int slaveId, RegisterType regType, int registerAddress, const std::vector<uint16_t>& values, const MqttPublishProps& responseProps)
            : MsgRegisterMessageBase(slaveId, regType, registerAddress), mValues(values), mResponseProps(responseProps) { }
        std::vector<uint16_t> mValues;
        MqttPublishProps mResponseProps;
};

/**
 * Sent back to reset availability
 * */
class MsgRegisterReadFailed : public MsgRegisterMessageBase {
    public:
        MsgRegisterReadFailed(int slaveId, RegisterType regType, int registerAddress)
            : MsgRegisterMessageBase(slaveId, regType, registerAddress)
        {}
};

/**
 * Sent back to reset availability
 * */
class MsgRegisterWriteFailed : public MsgRegisterMessageBase {
    public:
        MsgRegisterWriteFailed(int slaveId, RegisterType regType, int registerAddress)
            : MsgRegisterMessageBase(slaveId, regType, registerAddress)
        {}
};

/**
 * Sent back to close the Remote Call
 * */
class MsgRegisterRemoteCallResponseBase : public MsgRegisterMessageBase {
    public:
        MsgRegisterRemoteCallResponseBase(int slaveId, RegisterType regType, int registerAddress, const MqttPublishProps& props)
            : MsgRegisterMessageBase(slaveId, regType, registerAddress), mProps(props)
        {}
    MqttPublishProps mProps;
};

class MsgRegisterRemoteCallResponse : public MsgRegisterRemoteCallResponseBase {
    public:
        MsgRegisterRemoteCallResponse(int slaveId, RegisterType regType, int registerAddress, const MqttPublishProps& responseProps)
            : MsgRegisterRemoteCallResponseBase(slaveId, regType, registerAddress, responseProps)
        { }
        MsgRegisterRemoteCallResponse(int slaveId, RegisterType regType, int registerAddress, const MqttPublishProps& responseProps, const std::vector<uint16_t>& data)
            : MsgRegisterRemoteCallResponseBase(slaveId, regType, registerAddress, responseProps), mData(data)
        { }
    std::vector<uint16_t> mData;
};

class MsgRegisterRemoteCallError : public MsgRegisterRemoteCallResponseBase {
    public:
        MsgRegisterRemoteCallError(int slaveId, RegisterType regType, int registerAddress, const MqttPublishProps& responseProps, const std::exception& exc)
            : MsgRegisterRemoteCallResponseBase(slaveId, regType, registerAddress, responseProps), mError(exc.what())
        { }
    std::string mError;
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
