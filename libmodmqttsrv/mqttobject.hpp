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
        void addChildDataNode(const MqttObjectDataNode& pNode) { mNodes.push_back(pNode); }
        void setScalarNode(const MqttObjectRegisterIdent& ident);
        const std::vector<MqttObjectDataNode>& getChildNodes() const { return mNodes; }
        MqttValue getConvertedValue() const;
        uint16_t getRawValue() const;
    private:
        std::string mKeyName;
        std::vector<MqttObjectDataNode> mNodes;
        std::shared_ptr<MqttObjectRegisterIdent> mIdent;
        MqttObjectRegisterValue mValue;
        std::shared_ptr<DataConverter> mConverter;
};

class MqttObjectState {
    public:
        bool hasRegisterIn(const std::string& pNetworkName, const ModbusSlaveAddressRange& pRange) const;
        bool usesModbusNetwork(const std::string& networkName) const;
        bool updateRegisterValues(const std::string& pNetworkName, const MsgRegisterValues& pSlaveData);
        bool updateRegistersReadFailed(const std::string& pNetworkName, const ModbusSlaveAddressRange& pSlaveData);
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
        void setAvailableValue(const MqttValue& pValue) { mAvailableValue = pValue; }
    private:
        MqttValue mAvailableValue = 1;
};

class MqttObject {
    public:
        MqttObject(const std::string& pTopic);
        const std::string& getTopic() const { return mTopic; };
        const std::string& getAvailabilityTopic() const { return mAvailabilityTopic; }
        bool hasRegisterIn(const std::string& pNetworkName, const ModbusSlaveAddressRange& pRange) const;
        bool updateRegisterValues(const std::string& pNetworkName, const MsgRegisterValues& pSlaveData);
        bool updateRegistersReadFailed(const std::string& pNetworkName, const ModbusSlaveAddressRange& pSlaveData);
        bool setModbusNetworkState(const std::string& networkName, bool isUp);

        void addAvailabilityDataNode(const MqttObjectDataNode& pNode) { mAvailability.addDataNode(pNode); }
        void setAvailableValue(const MqttValue& pValue) { mAvailability.setAvailableValue(pValue); }
        AvailableFlag getAvailableFlag() const { return mIsAvailable; }

        MqttObjectState mState;

        void dump() const;
    private:
        std::string mTopic;
        std::string mAvailabilityTopic;

        MqttObjectAvailability mAvailability;

        AvailableFlag mIsAvailable = AvailableFlag::NotSet;
        void updateAvailablityFlag();

        void publish(const std::string& topic, const std::string& message);
        std::string convertToMessage(const MsgRegisterValues& pSlaveData);
};

} // namespace modmqttd
