#include <algorithm>
#include <iostream>

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "mqttobject.hpp"
#include "config.hpp"

namespace modmqttd {

bool MqttObjectRegisterValue::setValue(uint16_t val) {
    mHasValue = true;
    if (mValue != val) {
        mValue = val;
        return true;
    }
    return false;
}

bool MqttObjectDataNode::updateRegisterValues(const std::string& pNetworkName, const MsgRegisterValues& pSlaveData) {
    bool ret = false;
    if (!isScalar()) {
        for (MqttObjectDataNode& node : mNodes) {
            if (node.updateRegisterValues(pNetworkName, pSlaveData))
                ret = true;
        }
    } else {
        if (
            pSlaveData.mSlaveId == mIdent->mSlaveId &&
            pSlaveData.mRegisterType == mIdent->mRegisterType &&
            pNetworkName == mIdent->mNetworkName
        ) {
            // Check if our value is in pSlaveData range and update
            if (mIdent->mRegisterNumber >= pSlaveData.mRegister && mIdent->mRegisterNumber <= (pSlaveData.lastRegister())) {
                uint16_t idx = mIdent->mRegisterNumber - pSlaveData.mRegister;
                ret = mValue.setValue(pSlaveData.mRegisters.getValue(idx));
                mValue.setReadError(false);
            }
        }
    }
    return ret;
}

bool MqttObjectDataNode::updateRegistersReadFailed(const std::string& pNetworkName, const ModbusSlaveAddressRange& pSlaveData) {
    bool ret = false;
    if (!isScalar()) {
        for (MqttObjectDataNode& node : mNodes) {
            if (node.updateRegistersReadFailed(pNetworkName, pSlaveData))
                ret = true;
        }
    } else {
        if (
            pSlaveData.mSlaveId == mIdent->mSlaveId &&
            pSlaveData.mRegisterType == mIdent->mRegisterType &&
            pNetworkName == mIdent->mNetworkName
        ) {
            // Check if our value is in pSlaveData range and update
            uint16_t idx = abs(mIdent->mRegisterNumber - pSlaveData.mRegister);
            if (idx < pSlaveData.mCount) {
                mValue.setReadError(true);
                ret = true;
            }
        }
    }
    return ret;
}

bool MqttObjectDataNode::setModbusNetworkState(const std::string& pNetworkName, bool isUp) {
    bool ret = false;
    if (!isScalar()) {
        for (MqttObjectDataNode& node : mNodes) {
            if (node.setModbusNetworkState(pNetworkName, isUp))
                ret = true;
        }
    } else {
        if (pNetworkName == mIdent->mNetworkName) {
            if (mValue.isPolling() ^ isUp) {
                mValue.setReadError(!isUp);
                ret = true;
            }
        }
    }
    return ret;
}

bool MqttObjectDataNode::isPolling() const {
    if (isScalar())
        return mValue.isPolling();
    else {
        for (auto it = mNodes.begin(); it != mNodes.end(); it++)
            if (!it->isPolling()) {
                return false;
            }
    }
    return true;
}

AvailableFlag MqttObjectAvailability::getAvailableFlag() const {
    // No registers for availability
    // Assume that state is available
    if (mNodes.size() == 0)
        return AvailableFlag::True;

    // If we do not have all data
    // Then we do not know if state is available or not
    if (!hasAllValues() || !isPolling())
        return AvailableFlag::NotSet;

    // Single raw value or MqttObjectDataNode with converter and child nodes
    if (mNodes.front().getConvertedValue().getInt() != mAvailableValue.getInt())
        return AvailableFlag::False;
    return AvailableFlag::True;
}

void MqttObjectDataNode::setScalarNode(const MqttObjectRegisterIdent& ident) {
    mIdent.reset(new MqttObjectRegisterIdent(ident));
}

bool MqttObjectDataNode::hasRegisterIn(const std::string& pNetworkName, const ModbusSlaveAddressRange& pRange) const {
    if (isScalar()) {
        assert(mIdent != nullptr);
        if (mIdent->mSlaveId != pRange.mSlaveId)
            return false;
        if (!pRange.overlaps(mIdent->asModbusAddressRange()))
            return false;
        if (mIdent->mNetworkName != pNetworkName)
            return false;
        return true;
    } else {
        for (std::vector<MqttObjectDataNode>::const_iterator it = mNodes.begin(); it != mNodes.end(); it++) {
            if (it->hasRegisterIn(pNetworkName, pRange))
                return true;
        }
    }
    return false;
}

bool MqttObjectDataNode::hasAllValues() const {
    if (isScalar()) {
        assert(mIdent != nullptr);
        return mValue.hasValue();
    } else {
        for (std::vector<MqttObjectDataNode>::const_iterator it = mNodes.begin(); it != mNodes.end(); it++) {
            if (!it->hasAllValues())
                return false;
        }
    }
    return true;
}

MqttValue MqttObjectDataNode::getConvertedValue() const {
    if (mConverter != nullptr) {
        ModbusRegisters data;
        if (isScalar()) {
            data.appendValue(getRawValue());
        } else {
            for (std::vector<MqttObjectDataNode>::const_iterator it = mNodes.begin(); it != mNodes.end(); it++) {
                data.appendValue(it->getRawValue());
            }
        }
        return mConverter->toMqtt(data);
    } else {
        return MqttValue(mValue.getRawValue());
    }
}

uint16_t MqttObjectDataNode::getRawValue() const {
    assert(isScalar());
    return mValue.getRawValue();
}

bool MqttObjectState::hasRegisterIn(const std::string& pNetworkName, const ModbusSlaveAddressRange& pRange) const {
    for (std::vector<MqttObjectDataNode>::const_iterator it = mNodes.begin(); it != mNodes.end(); it++) {
        if (it->hasRegisterIn(pNetworkName, pRange))
            return true;
    }
    return false;
}

bool MqttObjectState::updateRegisterValues(const std::string& pNetworkName, const MsgRegisterValues& pSlaveData) {
    bool ret = false;
    for (std::vector<MqttObjectDataNode>::iterator it = mNodes.begin(); it != mNodes.end(); it++) {
        if (it->updateRegisterValues(pNetworkName, pSlaveData))
            ret = true;
    }
    return ret;
}

bool MqttObjectState::updateRegistersReadFailed(const std::string& pNetworkName, const ModbusSlaveAddressRange& pSlaveData) {
    bool ret = false;
    for (std::vector<MqttObjectDataNode>::iterator it = mNodes.begin(); it != mNodes.end(); it++) {
        if (it->updateRegistersReadFailed(pNetworkName, pSlaveData))
            ret = true;
    }
    return ret;
}

bool MqttObjectState::setModbusNetworkState(const std::string& networkName, bool isUp) {
    bool ret = false;
    for (std::vector<MqttObjectDataNode>::iterator it = mNodes.begin(); it != mNodes.end(); it++) {
        if (it->setModbusNetworkState(networkName, isUp))
            ret = true;
    }
    return ret;
}

bool MqttObjectState::hasAllValues() const {
    for (std::vector<MqttObjectDataNode>::const_iterator it = mNodes.begin(); it != mNodes.end(); it++) {
        if (!it->hasAllValues())
            return false;
    }
    return true;
}

bool MqttObjectState::isPolling() const {
    for (std::vector<MqttObjectDataNode>::const_iterator it = mNodes.begin(); it != mNodes.end(); it++) {
        if (!it->isPolling())
            return false;
    }
    return true;
}

MqttObject::MqttObject(const std::string& pTopic)
    : mTopic(pTopic),
      mAvailabilityTopic(pTopic + "/availability") {}

bool MqttObject::hasRegisterIn(const std::string& pNetworkName, const ModbusSlaveAddressRange& pRange) const {
    return mState.hasRegisterIn(pNetworkName, pRange) || mAvailability.hasRegisterIn(pNetworkName, pRange);
}

bool MqttObject::updateRegisterValues(const std::string& pNetworkName, const MsgRegisterValues& pSlaveData) {
    bool stateChanged = mState.updateRegisterValues(pNetworkName, pSlaveData);
    bool availChanged = mAvailability.updateRegisterValues(pNetworkName, pSlaveData);
    if (stateChanged || availChanged) {
        updateAvailablityFlag();

        // Преобразование данных в строку JSON или другой формат для публикации
        std::string message = convertToMessage(pSlaveData);

        // Публикация в mTopic
        publish(mTopic, message);

        return true;
    } else if (!mIsAvailable) {
        // Read was successful, so availability flag
        // may need to be updated if it depends only on state register data
        updateAvailablityFlag();
        return mIsAvailable;
    }
    return false;
}

bool MqttObject::updateRegistersReadFailed(const std::string& pNetworkName, const ModbusSlaveAddressRange& pSlaveData) {
    bool stateChanged = mState.updateRegistersReadFailed(pNetworkName, pSlaveData);
    bool availChanged = mAvailability.updateRegistersReadFailed(pNetworkName, pSlaveData);
    if (stateChanged || availChanged) {
        updateAvailablityFlag();

        // Преобразование данных об ошибке в строку JSON или другой формат для публикации
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "networkName" << YAML::Value << pNetworkName;
        out << YAML::Key << "slaveId" << YAML::Value << pSlaveData.mSlaveId;
        out << YAML::Key << "register" << YAML::Value << pSlaveData.mRegister;
        out << YAML::Key << "error" << YAML::Value << "read failed";
        out << YAML::EndMap;

        std::string message = out.c_str();

        // Публикация в mTopic
        publish(mTopic, message);

        return true;
    }
    return false;
}

bool MqttObject::setModbusNetworkState(const std::string& networkName, bool isUp) {
    bool stateChanged = mState.setModbusNetworkState(networkName, isUp);
    bool availChanged = mAvailability.setModbusNetworkState(networkName, isUp);
    if (stateChanged || availChanged) {
        updateAvailablityFlag();

        // Преобразование состояния сети в строку JSON или другой формат для публикации
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "networkName" << YAML::Value << networkName;
        out << YAML::Key << "isUp" << YAML::Value << isUp;
        out << YAML::EndMap;

        std::string message = out.c_str();

        // Публикация в mTopic
        publish(mTopic, message);

        return true;
    }
    return false;
}

void MqttObject::updateAvailablityFlag() {
    // If we cannot read availability registers
    // then assume that state data is invalid
    if (!mAvailability.isPolling() || !mState.isPolling())
        mIsAvailable = AvailableFlag::False;
    else {
        // We are reading all needed registers, check if
        // all of them have value
        if (!mAvailability.hasAllValues() || !mState.hasAllValues()) {
            mIsAvailable = AvailableFlag::NotSet;
        } else {
            // We have all values, check availability registers
            mIsAvailable = mAvailability.getAvailableFlag();
        }
    }
}

void MqttObject::publish(const std::string& topic, const std::string& message) {
    mqtt::client client("tcp://localhost:1883", "client_id");

    mqtt::message_ptr pubmsg = mqtt::make_message(topic, message);
    pubmsg->set_qos(1);

    client.connect();
    client.publish(pubmsg);
    client.disconnect();
}

std::string MqttObject::convertToMessage(const MsgRegisterValues& pSlaveData) {
    YAML::Emitter out;
    out << YAML::BeginMap;
    out << YAML::Key << "slaveId" << YAML::Value << pSlaveData.mSlaveId;
    out << YAML::Key << "values" << YAML::Value << YAML::Flow << pSlaveData.mValues;
    out << YAML::EndMap;
    return out.c_str();
}

} // namespace modmqttd
