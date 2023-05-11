#pragma once

#include <string>
#include <map>
#include <iostream>

#include <yaml-cpp/yaml.h>

#include "modbus_messages.hpp"
#include "libmodmqttconv/converter.hpp"

namespace modmqttd {

enum AvailableFlag {
    NotSet = -1,
    False = 0,
    True = 1
};

class MqttObjectRegisterIdent {
    public:
        struct Compare {
            bool operator() (const MqttObjectRegisterIdent& left, const MqttObjectRegisterIdent& right) const {
                return std::tie(left.mNetworkName, left.mSlaveId, left.mRegisterAddress, left.mRegisterType)
                        < std::tie(right.mNetworkName, right.mSlaveId, right.mRegisterAddress, right.mRegisterType);
            }
        };
        MqttObjectRegisterIdent(
            const std::string& network,
            int slaveId,
            RegisterType regType,
            int registerNumber
        ) : mNetworkName(network),
            mSlaveId(slaveId),
            mRegisterAddress(registerNumber),
            mRegisterType(regType)
        {}
        std::string mNetworkName;
        int mSlaveId;
        int mRegisterAddress;
        RegisterType mRegisterType;
};

class MqttObjectBase {
    private:
        size_t mTypeHash;
    public:
        enum PayloadType {
            STRING = 1
        };
        std::string mName;
        PayloadType mPayloadType;
        MqttObjectRegisterIdent mRegister;

        bool isSameAs(const std::type_info& type) const {
            return type.hash_code() == mTypeHash;
        }
    protected:
        MqttObjectBase(size_t typeHash, const std::string& name, const MqttObjectRegisterIdent& ident, PayloadType ptype)
            : mTypeHash(typeHash), mName(name), mRegister(ident), mPayloadType(ptype) {}
};

class MqttObjectCommand : public MqttObjectBase {
    public:
        MqttObjectCommand(const std::string& name, const MqttObjectRegisterIdent& ident, PayloadType ptype)
            : MqttObjectBase(typeid(MqttObjectCommand).hash_code(), name, ident, ptype) {}
};

class MqttObjectRemoteCall : public MqttObjectBase {
    public:
        MqttObjectRemoteCall(const std::string& name, const MqttObjectRegisterIdent& ident, PayloadType ptype, int size)
            : MqttObjectBase(typeid(MqttObjectRemoteCall).hash_code(), name, ident, ptype), mSize(size) {}
        int mSize;
};

class MqttObjectRegisterValue {
    public:
        MqttObjectRegisterValue(uint16_t val) : mValue(val), mHasValue(true), mReadOk(true) {}
        //no value yet, assume that read is in progress
        MqttObjectRegisterValue() : mHasValue(false), mReadOk(true) {}
        void setValue(uint16_t val) { mValue = val; mHasValue = true; }
        void setReadError(bool pFlag) { mReadOk = !pFlag; }
        void setConverter(std::shared_ptr<IStateConverter> conv) { mConverter = conv; }
        bool hasConverter() const { return mConverter != nullptr; }
        uint16_t getRawValue() const { return mValue; }
        MqttValue getConvertedValue() const;
        bool hasValue() const { return mHasValue; }
        bool isPolling() const { return mReadOk; }
    protected:
        bool mReadOk = false;
        bool mHasValue = false;
        uint16_t mValue;
        std::shared_ptr<IStateConverter> mConverter;
};

class MqttObjectAvailabilityValue : public MqttObjectRegisterValue {
    public:
        MqttObjectAvailabilityValue(uint16_t availValue = 1) : mAvailableValue(availValue), MqttObjectRegisterValue() {};
        AvailableFlag getAvailabilityFlag() const;
    private:
        uint16_t mAvailableValue;
};

template <typename T>
class MqttObjectRegisterHolder {
    public:
        void addRegister(const MqttObjectRegisterIdent& regIdent, const std::shared_ptr<IStateConverter>& conv);
        bool hasRegister(const MqttObjectRegisterIdent& regIdent) const;
        bool updateRegisterValue(const MqttObjectRegisterIdent& ident, uint16_t value);
        bool updateRegisterReadFailed(const MqttObjectRegisterIdent& regIdent);
        bool setModbusNetworkState(const std::string& networkName, bool isUp);
        bool hasValue() const;
        bool isPolling() const;
        MqttValue getFirstConvertedValue() const { return mRegisterValues.begin()->second.getConvertedValue(); }
        uint16_t getFirstRawValue() const { return mRegisterValues.begin()->second.getRawValue(); }
        const std::map<MqttObjectRegisterIdent, T, MqttObjectRegisterIdent::Compare>& getValues() const { return mRegisterValues; }
        ModbusRegisters getRawArray() const;
    protected:
        std::map<MqttObjectRegisterIdent, T, MqttObjectRegisterIdent::Compare> mRegisterValues;
};

class MqttObjectStateValue : public MqttObjectRegisterHolder<MqttObjectRegisterValue> {
    public:
        MqttObjectStateValue(const std::string& name, const MqttObjectRegisterIdent& ident, const std::shared_ptr<IStateConverter>& conv)
            : mName(name)
        {
            addRegister(ident, conv);
        }
        bool isUnnamed() const { return mName.empty(); }
        bool isScalar() const { return mRegisterValues.size() == 1; }
        std::string mName;
    private:
        std::shared_ptr<IStateConverter> mConverter;
};

class MqttObjectState {
    public:
        void addRegister(const std::string& name, const MqttObjectRegisterIdent& regIdent, const std::shared_ptr<IStateConverter>& conv);
        bool hasRegister(const MqttObjectRegisterIdent& regIdent) const;
        bool usesModbusNetwork(const std::string& networkName) const;
        bool updateRegisterValue(const MqttObjectRegisterIdent& ident, uint16_t value);
        bool updateRegisterReadFailed(const MqttObjectRegisterIdent& regIdent);
        bool setModbusNetworkState(const std::string& networkName, bool isUp);
        void setConverter(std::shared_ptr<IStateConverter> conv) { mConverter = conv; }
        std::string createMessage() const;
        bool hasValues() const;
        bool isPolling() const;
    private:
        std::vector<MqttObjectStateValue> mValues;
        std::shared_ptr<IStateConverter> mConverter;
};

class MqttObjectAvailability : public MqttObjectRegisterHolder<MqttObjectAvailabilityValue> {
    public:
        void addRegister(const MqttObjectRegisterIdent& regIdent, uint16_t availValue);
        AvailableFlag getAvailableFlag() const;
};

class MqttObject {
    public:
        MqttObject(const YAML::Node& data);
        const std::string& getTopic() const { return mTopic; };
        const std::string& getStateTopic() const { return mStateTopic; };
        const std::string& getAvailabilityTopic() const { return mAvailabilityTopic; }
        void updateRegisterValue(const MqttObjectRegisterIdent& regIdent, uint16_t value);
        void updateRegisterReadFailed(const MqttObjectRegisterIdent& regIdent);
        void setModbusNetworkState(const std::string& networkName, bool isUp);
        std::string createStateMessage();
        AvailableFlag getAvailableFlag() const { return mIsAvailable; }
        bool hasCommand(const std::string& name) const;

        std::vector<MqttObjectCommand> mCommands;
        std::vector<MqttObjectRemoteCall> mRemoteCalls;
        MqttObjectState mState;
        MqttObjectAvailability mAvailability;

        void dump() const;
    private:
        std::string mTopic;
        std::string mStateTopic;
        std::string mAvailabilityTopic;
        AvailableFlag mIsAvailable = AvailableFlag::NotSet;

        void updateAvailabilityFlag();
};

}
