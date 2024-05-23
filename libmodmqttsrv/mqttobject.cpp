#include <algorithm>
#include <iostream>

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "mqttobject.hpp"
#include "config.hpp"

namespace modmqttd {

/*MqttValue
MqttObjectRegisterValue::getConvertedValue() const {
    if (mConverter != nullptr) {
        ModbusRegisters data(mValue);
        return mConverter->toMqtt(data);
    } else {
        return MqttValue(mValue);
    }
}
*/

bool
MqttObjectRegisterValue::setValue(uint16_t val) {
    mHasValue = true;
    if (mValue != val) {
        mValue = val;
        return true;
    }
    return false;
}


/*
AvailableFlag
MqttObjectAvailabilityValue::getAvailabilityFlag() const {
    if (!mReadOk || !mHasValue)
        return AvailableFlag::NotSet;
    return mValue == mAvailableValue ? AvailableFlag::True : AvailableFlag::False;
};
*/

/*
template <typename T>
void
MqttObjectRegisterHolder<T>::addRegister(const MqttObjectRegisterIdent& regIdent, const std::shared_ptr<DataConverter>& conv) {
    auto it = mRegisterValues.find(regIdent);
    if (it == mRegisterValues.end()) {
        mRegisterValues[regIdent] = T();
        mRegisterValues[regIdent].setConverter(conv);
    }
}

template <typename T>
bool
MqttObjectRegisterHolder<T>::hasRegister(const MqttObjectRegisterIdent& regIdent) const {
    return mRegisterValues.find(regIdent) != mRegisterValues.end();
}
*/

bool
MqttObjectDataNode::updateRegisterValues(const std::string& pNetworkName, const MsgRegisterValues& pSlaveData) {
    bool ret = false;
    if (!isScalar()) {
        for(MqttObjectDataNode& node: mNodes) {
            if (node.updateRegisterValues(pNetworkName, pSlaveData))
                ret = true;
        }
    } else {
        if (
            pSlaveData.mSlaveId == mIdent->mSlaveId
            && pSlaveData.mRegisterType == mIdent->mRegisterType
            && pNetworkName == mIdent->mNetworkName
        ) {
            //check if our value is in pSlaveData range and update
            uint16_t idx = abs(mIdent->mRegisterNumber - pSlaveData.mRegister);
            if (idx < pSlaveData.mCount) {
                ret = mValue.setValue(pSlaveData.mRegisters.getValue(idx));
                mValue.setReadError(false);
            }
        }
    }
    return ret;
}

bool
MqttObjectDataNode::updateRegistersReadFailed(const std::string& pNetworkName, const MsgRegisterMessageBase& pSlaveData) {
    bool ret = false;
    if (!isScalar()) {
        for(MqttObjectDataNode& node: mNodes) {
            if (node.updateRegistersReadFailed(pNetworkName, pSlaveData))
                ret = true;
        }
    } else {
        if (
            pSlaveData.mSlaveId == mIdent->mSlaveId
            && pSlaveData.mRegisterType == mIdent->mRegisterType
            && pNetworkName == mIdent->mNetworkName
        ) {
            //check if our value is in pSlaveData range and update
            uint16_t idx = abs(mIdent->mRegisterNumber - pSlaveData.mRegister);
            if (idx < pSlaveData.mCount) {
                mValue.setReadError(true);
                ret = true;
            }
        }
    }
    return ret;
}

bool
MqttObjectDataNode::setModbusNetworkState(const std::string& pNetworkName, bool isUp) {
    bool ret = false;
    if (!isScalar()) {
        for(MqttObjectDataNode& node: mNodes) {
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


/*
template <typename T>
bool
MqttObjectRegisterHolder<T>::setModbusNetworkState(const std::string& networkName, bool isUp) {
    bool ret = false;
    for(auto it = mRegisterValues.begin(); it != mRegisterValues.end(); it++) {
        if (it->first.mNetworkName == networkName) {
            it->second.setReadError(!isUp);
            ret = true;
        }
    }
    return ret;
}

template <typename T>
bool
MqttObjectRegisterHolder<T>::hasValue() const {
    for(auto it = mRegisterValues.begin(); it != mRegisterValues.end(); it++)
        if (!it->second.hasValue()) {
            return false;
        }
    return true;
}
*/

bool
MqttObjectDataNode::isPolling() const {
    if (isScalar())
        return mValue.isPolling();
    else {
        for(auto it = mNodes.begin(); it != mNodes.end(); it++)
            if (!it->isPolling()) {
                return false;
            }
    }
    return true;
}

/*
template <typename T>
ModbusRegisters
MqttObjectRegisterHolder<T>::getRawArray() const {
    ModbusRegisters ret;

    for(auto it = mRegisterValues.begin(); it != mRegisterValues.end(); it++)
        ret.appendValue(it->second.getRawValue());

    return ret;
}
*/
/*
void
MqttObjectAvailability::addRegister(const MqttObjectRegisterIdent& regIdent, uint16_t availValue) {
    mRegisterValues[regIdent] = MqttObjectAvailabilityValue(availValue);
};
*/

AvailableFlag
MqttObjectAvailability::getAvailableFlag() const {
    //TODO compare or use converter
    /*
    for(std::map<MqttObjectRegisterIdent, MqttObjectAvailabilityValue>::const_iterator it = mNodes.begin(); it != mNodes.end(); it++) {
        switch(it->second.getAvailabilityFlag()) {
            case AvailableFlag::NotSet:
                return AvailableFlag::NotSet;
            break;
            case AvailableFlag::False:
                return AvailableFlag::False;
            break;
            case AvailableFlag::True:
                continue;
            break;
        }
    }*/

    // no registers for availability
    // assume that state is available
    if (mNodes.size() == 0)
        return AvailableFlag::True;

    // if we do not have all data
    // then we do not know if state is available or not
    if (!hasAllValues() || !isPolling())
        return AvailableFlag::NotSet;

    // single raw value or MqttObjectDataNode with converter and child nodes
    if (mNodes.front().getConvertedValue().getInt() != mAvailableValue.getInt())
        return AvailableFlag::False;
    return AvailableFlag::True;
}

/*
void
MqttObjectState::addRegister(const std::string& name, const MqttObjectRegisterIdent& regIdent, const std::shared_ptr<DataConverter>& conv) {
    std::vector<MqttObjectStateValue>::iterator existing = std::find_if(
        mValues.begin(),
        mValues.end(),
        [&name](const MqttObjectStateValue& item) -> bool { return item.mName == name; }
    );
    if (existing == mValues.end()) {
        mValues.push_back(MqttObjectStateValue(name, regIdent, conv));
    } else {
        existing->addRegister(regIdent, conv);
    }
};


void
createConvertedValue(
    rapidjson::Writer<rapidjson::StringBuffer>& writer,
    const MqttValue& value
) {
    switch(value.getSourceType()) {
        case MqttValue::SourceType::INT:
            writer.Int(value.getInt());
            break;
        case MqttValue::SourceType::DOUBLE: {
            int prec = value.getDoublePrecision();
            if (prec != MqttValue::NO_PRECISION)
                writer.SetMaxDecimalPlaces(prec);
            writer.Double(value.getDouble());
            break;
        }
        case MqttValue::SourceType::BINARY:
            writer.String(static_cast<const char*>(value.getBinaryPtr()), value.getBinarySize());
            break;
        case MqttValue::SourceType::INT64:
            writer.Int64(value.getInt64());
            break;
    }
}

void
createRegisterValuesArray(
    rapidjson::Writer<rapidjson::StringBuffer>& writer,
    const std::map<MqttObjectRegisterIdent,
                    MqttObjectRegisterValue,
                    MqttObjectRegisterIdent::Compare>
                mRegisterValues)
{
    writer.StartArray();
    for(auto regValue = mRegisterValues.begin();
        regValue != mRegisterValues.end(); regValue++)
    {
        if (regValue->second.hasConverter()) {
            createConvertedValue(writer, regValue->second.getConvertedValue());
        } else {
            writer.Uint(regValue->second.getRawValue());
        }
    }
    writer.EndArray();
}
*/

void
MqttObjectDataNode::setScalarNode(const MqttObjectRegisterIdent& ident) {
    mIdent.reset(new MqttObjectRegisterIdent(ident));
}

bool
MqttObjectDataNode::hasRegisterIn(const std::string& pNetworkName, const MsgRegisterPoll& pPoll) const {
    if (isScalar()) {
        assert(mIdent != nullptr);
        if (mIdent->mSlaveId != pPoll.mSlaveId)
            return false;
        if (!pPoll.overlaps(mIdent->asModbusAddressRange()))
            return false;
        if (mIdent->mNetworkName != pNetworkName)
            return false;
        return true;
    } else {
        for(std::vector<MqttObjectDataNode>::const_iterator it = mNodes.begin(); it != mNodes.end(); it++) {
            if (it->hasRegisterIn(pNetworkName, pPoll))
                return true;
        }
    }
    return false;
}

bool
MqttObjectDataNode::hasAllValues() const {
    if (isScalar()) {
        assert(mIdent != nullptr);
        return mValue.hasValue();
    } else {
        for(std::vector<MqttObjectDataNode>::const_iterator it = mNodes.begin(); it != mNodes.end(); it++) {
            if (!it->hasAllValues())
                return false;
        }
    }
    return true;
}

MqttValue
MqttObjectDataNode::getConvertedValue() const {
    if (mConverter != nullptr) {
        ModbusRegisters data;
        if (isScalar()) {
            data.appendValue(getRawValue());
        } else {
            for(std::vector<MqttObjectDataNode>::const_iterator it = mNodes.begin(); it != mNodes.end(); it++) {
                data.appendValue(it->getRawValue());
            }
        }
        return mConverter->toMqtt(data);
    } else {
        return MqttValue(mValue.getRawValue());
    }
}

uint16_t
MqttObjectDataNode::getRawValue() const {
    assert(isScalar());
    return mValue.getRawValue();
}

/*
void
createConvertedValue(
    rapidjson::Writer<rapidjson::StringBuffer>& writer,
    const ModbusRegisters& registers,
    const DataConverter& converter
) {
    MqttValue v = converter.toMqtt(registers);
    createConvertedValue(writer, v);
}

void
createConvertedValue(
    rapidjson::Writer<rapidjson::StringBuffer>& writer,
    const MqttObjectStateValue& stateValue,
    const std::shared_ptr<DataConverter>& converter
) {
    if (stateValue.isScalar()) {
        //single value
        const MqttObjectRegisterValue& val(stateValue.getValues().begin()->second);
        if (converter != nullptr)
            createConvertedValue(writer, converter->toMqtt(val.getRawValue()));
        else {
            if (val.hasConverter())
                createConvertedValue(writer, val.getConvertedValue());
            else
                writer.Uint(val.getRawValue());
        }
    } else {
        //array of values
        if (converter != nullptr)
            createConvertedValue(writer, stateValue.getRawArray(), *converter);
        else
            createRegisterValuesArray(writer, stateValue.getValues());
    }
}

std::string
MqttObjectState::createMessage() const {
    // return mqtt value without rapidjson
    // processing
    if (mValues.size() == 1) {
        const MqttObjectStateValue& single(mValues[0]);
        if (single.isUnnamed() && (single.isScalar() || mConverter != nullptr)) {
            if (mConverter != nullptr) {
                MqttValue v = mConverter->toMqtt(single.getRawArray());
                return v.getString();
            } else {
                return std::to_string(single.getFirstRawValue());
            }
        }
    }

    //in all other cases we output json string
    {
        const MqttObjectStateValue& first(mValues[0]);
        rapidjson::StringBuffer ret;
        rapidjson::Writer<rapidjson::StringBuffer> writer(ret);
        if (mValues.size() == 1) {
            if (first.isUnnamed()) {
                //unnamed array, single value is handled above
                createRegisterValuesArray(writer, first.getValues());
            } else {
                writer.StartObject();
                writer.Key(first.mName.c_str());
                //for named scalar or named array
                createConvertedValue(writer, first, mConverter);
                writer.EndObject();
            }
        } else {
            if (first.isUnnamed()) {
                //unnamed array, assume all StateValue objects are unnamed
                writer.StartArray();
                for(auto it = mValues.begin(); it != mValues.end(); it++) {
                    createConvertedValue(writer, *it, mConverter);
                }
                writer.EndArray();
            } else {
                //map, assume all StateValue objects have a name
                writer.StartObject();
                for(auto it = mValues.begin(); it != mValues.end(); it++) {
                    writer.Key(it->mName.c_str());
                    createConvertedValue(writer, *it, mConverter);
                }
                writer.EndObject();
            };
        }
        return ret.GetString();
    }
}



*/


bool
MqttObjectState::hasRegisterIn(const std::string& pNetworkName, const MsgRegisterPoll& pPoll) const {
    for(std::vector<MqttObjectDataNode>::const_iterator it = mNodes.begin(); it != mNodes.end(); it++) {
        if (it->hasRegisterIn(pNetworkName, pPoll))
            return true;
    }
    return false;
}


bool
MqttObjectState::updateRegisterValues(const std::string& pNetworkName, const MsgRegisterValues& pSlaveData) {
    bool ret = false;
    for(std::vector<MqttObjectDataNode>::iterator it = mNodes.begin(); it != mNodes.end(); it++) {
        if (it->updateRegisterValues(pNetworkName, pSlaveData))
            ret = true;
    }
    return ret;
}

bool
MqttObjectState::updateRegistersReadFailed(const std::string& pNetworkName, const MsgRegisterMessageBase& pSlaveData) {
    bool ret = false;
    for(std::vector<MqttObjectDataNode>::iterator it = mNodes.begin(); it != mNodes.end(); it++) {
        if (it->updateRegistersReadFailed(pNetworkName, pSlaveData))
            ret = true;
    }
    return ret;
}

bool
MqttObjectState::setModbusNetworkState(const std::string& networkName, bool isUp) {
    bool ret = false;
    for(std::vector<MqttObjectDataNode>::iterator it = mNodes.begin(); it != mNodes.end(); it++) {
        if (it->setModbusNetworkState(networkName, isUp))
            ret = true;
    }
    return ret;
}

bool
MqttObjectState::hasAllValues() const {
    for(std::vector<MqttObjectDataNode>::const_iterator it = mNodes.begin(); it != mNodes.end(); it++) {
        if (!it->hasAllValues())
            return false;
    }
    return true;
}

bool
MqttObjectState::isPolling() const {
    for(std::vector<MqttObjectDataNode>::const_iterator it = mNodes.begin(); it != mNodes.end(); it++) {
        if (!it->isPolling())
            return false;
    }
    return true;
}


MqttObject::MqttObject(const std::string& pTopic)
    : mTopic(pTopic)
{
    mStateTopic = mTopic + "/state";
    mAvailabilityTopic = mTopic + "/availability";
};

bool
MqttObject::hasRegisterIn(const std::string& pNetworkName, const MsgRegisterPoll& pPoll) const {
    return mState.hasRegisterIn(pNetworkName, pPoll) || mAvailability.hasRegisterIn(pNetworkName, pPoll);
}


bool
MqttObject::updateRegisterValues(const std::string& pNetworkName, const MsgRegisterValues& pSlaveData) {
    bool stateChanged = mState.updateRegisterValues(pNetworkName, pSlaveData);
    bool availChanged = mAvailability.updateRegisterValues(pNetworkName, pSlaveData);
    if (stateChanged || availChanged) {
        updateAvailablityFlag();
        return true;
    } else if (!mIsAvailable) {
        // read was successfull, so availability flag
        // may need to be updated if it depends only on state register data
        // avaiablity.
        updateAvailablityFlag();
        return mIsAvailable;
    }
    return false;
}

bool
MqttObject::updateRegistersReadFailed(const std::string& pNetworkName, const MsgRegisterMessageBase& pSlaveData) {
    bool stateChanged = mState.updateRegistersReadFailed(pNetworkName, pSlaveData);
    bool availChanged = mAvailability.updateRegistersReadFailed(pNetworkName, pSlaveData);
    if (stateChanged || availChanged) {
        updateAvailablityFlag();
        return true;
    }
    return false;
}

bool
MqttObject::setModbusNetworkState(const std::string& networkName, bool isUp) {
    bool stateChanged = mState.setModbusNetworkState(networkName, isUp);
    bool availChanged = mAvailability.setModbusNetworkState(networkName, isUp);
    if (stateChanged || availChanged) {
        updateAvailablityFlag();
        return true;
    }
    return false;
}

void
MqttObject::updateAvailablityFlag() {
    // if we cannot read availability registers
    // then assume that state data is invalid
    if (!mAvailability.isPolling() || !mState.isPolling())
        mIsAvailable = AvailableFlag::False;
    else {
        //we are reading all needed registers, check if
        //all of them have value
        if (!mAvailability.hasAllValues() || !mState.hasAllValues()) {
            mIsAvailable = AvailableFlag::NotSet;
        } else {
            //we have all values, check avaiabilty registers
            mIsAvailable = mAvailability.getAvailableFlag();
        }
    }
}


} //namespace
