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
        struct Equal {
            bool operator() (const MqttObjectRegisterIdent& left, const MqttObjectRegisterIdent& right) const {
                return left.mSlaveId == right.mSlaveId
                    && left.mRegisterNumber == right.mRegisterNumber
                    && left.mRegisterType == right.mRegisterType
                    && left.mNetworkName == right.mNetworkName;
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

        bool operator==(const MqttObjectRegisterIdent& other) {
            return Equal()(*this, other);
        }

        ModbusAddressRange asModbusAddressRange() const {
            return ModbusAddressRange(mRegisterNumber, mRegisterType, 1);
        }

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
        MqttObjectRegisterValue() : mHasValue(false), mReadOk(true) {}
        bool setValue(uint16_t val);
        void clearValue() { mHasValue = false; }
        void setReadError(bool pFlag) { mReadOk = !pFlag; }
        //void setConverter(std::shared_ptr<DataConverter> conv) { mConverter = conv; }
        //bool hasConverter() const { return mConverter != nullptr; }
        uint16_t getRawValue() const { return mValue; }
        //MqttValue getConvertedValue() const;
        bool hasValue() const { return mHasValue; }
        bool isPolling() const { return mReadOk; }
    protected:
        bool mReadOk = false;
        bool mHasValue = false;
        uint16_t mValue;
        //std::shared_ptr<DataConverter> mConverter;
};

/*
class MqttObjectAvailabilityValue : public MqttObjectRegisterValue {
    public:
        MqttObjectAvailabilityValue(uint16_t availValue = 1) : mAvailableValue(availValue), MqttObjectRegisterValue() {};
        AvailableFlag getAvailabilityFlag() const;
    private:
        // expected register value if register is available
        uint16_t mAvailableValue;
};
*/
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
        bool updateRegisterValues(const std::string& pNetworkName, const MsgRegisterValues& pSlaveData);
        bool updateRegistersReadFailed(const std::string& pNetworkName, const MsgRegisterMessageBase& pSlaveData);
        bool setModbusNetworkState(const std::string& networkName, bool isUp);

        bool hasRegisterIn(const std::string& pNetworkName, const MsgRegisterPoll& pPoll) const;
        bool hasAllValues() const;
        bool isPolling() const;

        bool isUnnamed() const { return mKeyName.empty(); }
        void setName(const std::string& pName) { mKeyName = pName; }
        const std::string& getName() const { return mKeyName; }

        void setConverter(std::shared_ptr<DataConverter> conv) { mConverter = conv; }
        bool hasConverter() const { return mConverter != nullptr; }

        bool isScalar() const { return mNodes.size() == 0; }
        void addChildDataNode(const MqttObjectDataNode& pNode) { mNodes.push_back(pNode); }
        void setScalarNode(const MqttObjectRegisterIdent& ident);
        const std::vector<MqttObjectDataNode>& getChildNodes() const { return mNodes; }
        MqttValue getConvertedValue() const;
        uint16_t getRawValue() const;
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
         * Modbus register identifier used to
         * update values received from modbus threads.
        */
        std::shared_ptr<MqttObjectRegisterIdent> mIdent;
        /**
         * if mNodes is empty then this is a scalar value.
        */
        MqttObjectRegisterValue mValue;

        /**
         * A converter used to convert mValue or list of scalars on mNodes list
        */
        std::shared_ptr<DataConverter> mConverter;
};

class MqttObjectState {
    public:
        //void addRegister(const std::string& name, const MqttObjectRegisterIdent& regIdent, const std::shared_ptr<DataConverter>& conv);
        bool hasRegisterIn(const std::string& pNetworkName, const MsgRegisterPoll& pPoll) const;
        bool usesModbusNetwork(const std::string& networkName) const;
        bool updateRegisterValues(const std::string& pNetworkName, const MsgRegisterValues& pSlaveData);
        bool updateRegistersReadFailed(const std::string& pNetworkName, const MsgRegisterMessageBase& pSlaveData);
        bool setModbusNetworkState(const std::string& networkName, bool isUp);
        bool hasAllValues() const;
        bool isPolling() const;
        void addDataNode(const MqttObjectDataNode& pNode) { mNodes.push_back(pNode); }
        const std::vector<MqttObjectDataNode>& getNodes() const { return mNodes; }
    protected:
        std::vector<MqttObjectDataNode> mNodes;
};

class MqttObjectAvailability : public MqttObjectState {
    public:
        AvailableFlag getAvailableFlag() const;
        void setAvailableValue(uint16_t pValue) { mAvailableValue = pValue; }
    private:
        MqttValue mAvailableValue = 1;
};

class MqttObject {
    public:
        MqttObject(const std::string& pTopic);
        const std::string& getTopic() const { return mTopic; };
        const std::string& getStateTopic() const { return mStateTopic; };
        const std::string& getAvailabilityTopic() const { return mAvailabilityTopic; }
        bool hasRegisterIn(const std::string& pNetworkName, const MsgRegisterPoll& pPoll) const;
        bool updateRegisterValues(const std::string& pNetworkName, const MsgRegisterValues& pSlaveData);
        bool updateRegistersReadFailed(const std::string& pNetworkName, const MsgRegisterMessageBase& pSlaveData);
        bool setModbusNetworkState(const std::string& networkName, bool isUp);

        void addAvailabilityDataNode(const MqttObjectDataNode& pNode) { mAvailability.addDataNode(pNode); }
        void setAvailableValue(uint16_t pValue) { mAvailability.setAvailableValue(pValue); }
        AvailableFlag getAvailableFlag() const { return mIsAvailable; }

        MqttObjectState mState;

        void dump() const;
    private:
        std::string mTopic;
        std::string mStateTopic;
        std::string mAvailabilityTopic;

        MqttObjectAvailability mAvailability;

        AvailableFlag mIsAvailable = AvailableFlag::NotSet;
        void updateAvailablityFlag();
};

}
