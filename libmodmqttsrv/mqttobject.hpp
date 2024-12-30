#pragma once

#include <string>
#include <map>
#include <iostream>

#include <yaml-cpp/yaml.h>

#include "modbus_messages.hpp"
#include "common.hpp"
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

        MqttObjectRegisterIdent(const std::string& network, const ModbusSlaveAddressRange& slaveData)
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


class MqttObjectRegisterValue {
    public:
        MqttObjectRegisterValue() : mHasValue(false), mReadOk(true) {}
        bool setValue(uint16_t val);
        void clearValue() { mHasValue = false; }
        void setReadError(bool pFlag) { mReadOk = !pFlag; }
        uint16_t getRawValue() const { return mValue; }
        bool hasValue() const { return mHasValue; }
        bool isPolling() const { return mReadOk; }
    protected:
        bool mReadOk = false;
        bool mHasValue = false;
        uint16_t mValue;
};

class MqttObjectDataNode;

/**
 * A MqttObjectDataNode vector with a flag
 * telling if vector was created from single yaml object
 * or a yaml list. Needed to for mqttpayload to handle
 * lists with single elements
 */
class MqttObjectDataNodeList : public std::vector<MqttObjectDataNode> {
    public:
        void forceListOutput(bool flag) { mForceListOutput = flag; }
        bool outputAsList() const { return mForceListOutput; }
    private:
        bool mForceListOutput = false;
};


class MqttObjectDataNode {
    public:
        bool updateRegisterValues(const std::string& pNetworkName, const MsgRegisterValues& pSlaveData);
        bool updateRegistersReadFailed(const std::string& pNetworkName, const ModbusSlaveAddressRange& pSlaveData);
        bool setModbusNetworkState(const std::string& networkName, bool isUp);

        bool hasRegisterIn(const std::string& pNetworkName, const ModbusSlaveAddressRange& pRange) const;
        bool hasAllValues() const;
        bool isPolling() const;

        bool isUnnamed() const { return mKeyName.empty(); }
        void setName(const std::string& pName) { mKeyName = pName; }
        const std::string& getName() const { return mKeyName; }

        void setConverter(std::shared_ptr<DataConverter> conv) { mConverter = conv; }
        bool hasConverter() const { return mConverter != nullptr; }

        bool isScalar() const { return mNodes.size() == 0; }
        void addChildDataNode(const MqttObjectDataNode& pNode, bool forceList = false);
        void setScalarNode(const MqttObjectRegisterIdent& ident);
        const MqttObjectDataNodeList& getChildNodes() const { return mNodes; }
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

        MqttObjectDataNodeList mNodes;

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
        bool hasRegisterIn(const std::string& pNetworkName, const ModbusSlaveAddressRange& pRange) const;
        bool usesModbusNetwork(const std::string& networkName) const;
        bool updateRegisterValues(const std::string& pNetworkName, const MsgRegisterValues& pSlaveData);
        bool updateRegistersReadFailed(const std::string& pNetworkName, const ModbusSlaveAddressRange& pSlaveData);
        bool setModbusNetworkState(const std::string& networkName, bool isUp);
        bool hasAllValues() const;
        bool isPolling() const;
        void addDataNode(const MqttObjectDataNode& pNode, bool forceList = false);
        const MqttObjectDataNodeList& getNodes() const { return mNodes; }
    protected:
        MqttObjectDataNodeList mNodes;
};


class MqttObjectAvailability : public MqttObjectState {
    public:
        AvailableFlag getAvailableFlag() const;
        void setAvailableValue(const MqttValue& pValue) { mAvailableValue = pValue; }
    private:
        MqttValue mAvailableValue = 1;
};


class MqttObject {
    public:
        MqttObject(const std::string& pTopic);
        const std::string& getTopic() const { return mTopic; };
        const std::string& getStateTopic() const { return mStateTopic; };
        const std::string& getAvailabilityTopic() const { return mAvailabilityTopic; }
        bool hasRegisterIn(const std::string& pNetworkName, const ModbusSlaveAddressRange& pRange) const;
        void updateRegisterValues(const std::string& pNetworkName, const MsgRegisterValues& pSlaveData);
        void updateRegistersReadFailed(const std::string& pNetworkName, const ModbusSlaveAddressRange& pSlaveData);
        bool setModbusNetworkState(const std::string& networkName, bool isUp);

        void addAvailabilityDataNode(const MqttObjectDataNode& pNode) { mAvailability.addDataNode(pNode); }
        void setAvailableValue(const MqttValue& pValue) { mAvailability.setAvailableValue(pValue); }
        AvailableFlag getAvailableFlag() const { return mIsAvailable; }

        bool hasValue() const { return mState.hasAllValues(); }

        void setLastPublishedPayload(const std::string& pVal) { mLastPublishedPayload = pVal; }
        const std::string& getLastPublishedPayload() const { return mLastPublishedPayload; }

        void setPublishMode(const PublishMode& pMode) { mPublishMode = pMode; }
        const PublishMode& getPublishMode() const { return mPublishMode; }

        void setRetain(bool pFlag) { mRetain = pFlag; }
        bool getRetain() const { return mRetain; }

        MqttObjectState mState;

        void dump() const;
    private:
        std::string mTopic;
        std::string mStateTopic;
        std::string mAvailabilityTopic;

        MqttObjectAvailability mAvailability;

        AvailableFlag mIsAvailable = AvailableFlag::NotSet;

        bool mRetain = true;
        PublishMode mPublishMode;
        std::string mLastPublishedPayload;

        void updateAvailablityFlag();
};


}
