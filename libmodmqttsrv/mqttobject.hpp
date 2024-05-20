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
                return std::tie(left.mNetworkName, left.mSlaveId, left.mRegisterNumber, left.mRegisterType)
                        < std::tie(right.mNetworkName, right.mSlaveId, right.mRegisterNumber, right.mRegisterType);
            }
        };
        MqttObjectRegisterIdent(
            const std::string& network,
            int slaveId,
            RegisterType regType,
            int registerNumber
        ) : mNetworkName(network),
            mSlaveId(slaveId),
            mRegisterNumber(registerNumber),
            mRegisterType(regType)
        {}

        MqttObjectRegisterIdent(const std::string& network, const MsgRegisterMessageBase& slaveData)
          : mNetworkName(network),
            mSlaveId(slaveData.mSlaveId),
            mRegisterNumber(slaveData.mRegister),
            mRegisterType(slaveData.mRegisterType)
        {}

        std::string mNetworkName;
        int mSlaveId;
        int mRegisterNumber;
        RegisterType mRegisterType;
};

class MqttObjectCommand {
    public:
        enum PayloadType {
            STRING = 1
        };
        MqttObjectCommand(const std::string& topic, const MqttObjectRegisterIdent& ident, PayloadType ptype, int register_count = 1)
            : mTopic(topic), mRegister(ident), mPayloadType(ptype), mCount(register_count) {}
        std::string mTopic;
        PayloadType mPayloadType;
        MqttObjectRegisterIdent mRegister;
        int mCount;

        void setConverter(std::shared_ptr<DataConverter> conv) { mConverter = conv; }
        bool hasConverter() const { return mConverter != nullptr; }
        const DataConverter& getConverter() const { return *mConverter; }
    private:
        std::shared_ptr<DataConverter> mConverter;
};

class MqttObjectRegisterValue {
    public:
        MqttObjectRegisterValue(uint16_t val) : mValue(val), mHasValue(true), mReadOk(true) {}
        //no value yet, assume that read is in progress
        MqttObjectRegisterValue() : mHasValue(false), mReadOk(true) {}
        void setValue(uint16_t val) { mValue = val; mHasValue = true; }
        void setReadError(bool pFlag) { mReadOk = !pFlag; }
        void setConverter(std::shared_ptr<DataConverter> conv) { mConverter = conv; }
        bool hasConverter() const { return mConverter != nullptr; }
        uint16_t getRawValue() const { return mValue; }
        MqttValue getConvertedValue() const;
        bool hasValue() const { return mHasValue; }
        bool isPolling() const { return mReadOk; }
    protected:
        bool mReadOk = false;
        bool mHasValue = false;
        uint16_t mValue;
        std::shared_ptr<DataConverter> mConverter;
};

class MqttObjectAvailabilityValue : public MqttObjectRegisterValue {
    public:
        MqttObjectAvailabilityValue(uint16_t availValue = 1) : mAvailableValue(availValue), MqttObjectRegisterValue() {};
        AvailableFlag getAvailabilityFlag() const;
    private:
        // expected register value if register is available
        uint16_t mAvailableValue;
};

/*!
    Base class for state an availability

template <typename T>
class MqttObjectRegisterHolder {
    public:
        void addRegister(const MqttObjectRegisterIdent& regIdent, const std::shared_ptr<DataConverter>& conv);
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
*/

class MqttObjectDataNode {
    public:
        void init(
            const YAML::Node& pNode, 
            const std::string& pDefaultNetwork, 
            int pDefaultSlave, 
            std::chrono::milliseconds> refresh, 
            std::vector<MsgRegisterPollSpecification>& pSpecs
        );
        bool isUnnamed() const { return mKeyName.empty(); }
        bool isScalar() const { return mNodes.size() == 0; }
    private:
        // if not empty then json value is published as json object
        // 
        std::string mKeyName;
        /**
         * if not empty then this is composite node
         * that is published as:
         *  - an object if all mNodes have mKeyName
         *  - a list if all mNodes have mKeyName empty
         *  - a single value if mConverter is set. In this case
         *    mNodes list must hold scalars only.
         * */ 

        std::vector<MqttObjectDataNode> mNodes;
        /**
         * if mNodes is empty then this is a scalar value.
        */
        MqttObjectRegisterValue mValue;

        /**
         * Modbus register identifier used to 
         * update values received from modbus threads.
        */
        MqttObjectRegisterIdent mIdent;
        /**
         * A converter used to convert mValue or list of scalars on mNodes list 
        */
        std::shared_ptr<DataConverter> mConverter;
};

class MqttObjectState {
    public:
        void init(
            const YAML::Node& pStateNode, 
            const std::string& pDefaultNetwork, 
            int pDefaultSlave, 
            std::chrono::milliseconds> refresh, 
            std::vector<MsgRegisterPollSpecification>& pSpecs
        );
        //void addRegister(const std::string& name, const MqttObjectRegisterIdent& regIdent, const std::shared_ptr<DataConverter>& conv);
        bool hasRegister(const MqttObjectRegisterIdent& regIdent) const;
        bool usesModbusNetwork(const std::string& networkName) const;
        bool updateRegisterValue(const MqttObjectRegisterIdent& ident, uint16_t value);
        bool updateRegisterReadFailed(const MqttObjectRegisterIdent& regIdent);
        bool setModbusNetworkState(const std::string& networkName, bool isUp);
        //void setConverter(std::shared_ptr<DataConverter> conv) { mConverter = conv; }
        std::string createMessage() const;
        bool hasValues() const;
        bool isPolling() const;
    private:
        std::vector<MqttObjectDataNode> mNodes;
        /**
         * A converter used to convert a list of scalars on mNodes list 
        */        
        std::shared_ptr<DataConverter> mConverter;
};

class MqttObjectAvailability : public MqttObjectState {
    public:
        AvailableFlag getAvailableFlag() const;
    private:
        AvailableFlag mIsAvailable = AvailableFlag::NotSet;
        void updateAvailablityFlag(); // from MqttObject ?        
};

class MqttObject {
    public:
        MqttObject(const YAML::Node& data);
        const std::string& getTopic() const { return mTopic; };
        const std::string& getStateTopic() const { return mStateTopic; };
        const std::string& getAvailabilityTopic() const { return mAvailabilityTopic; }
        bool updateRegisterValue(const MqttObjectRegisterIdent& regIdent, uint16_t value);
        bool updateRegisterReadFailed(const MqttObjectRegisterIdent& regIdent);
        bool setModbusNetworkState(const std::string& networkName, bool isUp);
        std::string createStateMessage();

        MqttObjectState mState;
        MqttObjectAvailability mAvailability;

        void dump() const;
    private:
        std::string mTopic;
        std::string mStateTopic;
        std::string mAvailabilityTopic;
};

}
