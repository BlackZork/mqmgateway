#include <algorithm>
#include <iostream>

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "mqttobject.hpp"
#include "config.hpp"

namespace modmqttd {

MqttValue
MqttObjectRegisterValue::getConvertedValue() const {
    ModbusRegisters data(mValue);
    return mConverter->toMqtt(data);
}

AvailableFlag
MqttObjectAvailabilityValue::getAvailabilityFlag() const {
    if (!mReadOk || !mHasValue)
        return AvailableFlag::NotSet;
    return mValue == mAvailableValue ? AvailableFlag::True : AvailableFlag::False;
};

template <typename T>
void
MqttObjectRegisterHolder<T>::addRegister(const MqttObjectRegisterIdent& regIdent, const std::shared_ptr<IStateConverter>& conv) {
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

template <typename T>
bool
MqttObjectRegisterHolder<T>::updateRegisterValue(const MqttObjectRegisterIdent& ident, uint16_t value) {
    auto reg = mRegisterValues.find(ident);
    if (reg != mRegisterValues.end()) {
        reg->second.setReadError(false);
        reg->second.setValue(value);
        return true;
    }
    return false;
}

template <typename T>
bool
MqttObjectRegisterHolder<T>::updateRegisterReadFailed(const MqttObjectRegisterIdent& regIdent) {
    auto reg = mRegisterValues.find(regIdent);
    if (reg != mRegisterValues.end()) {
        reg->second.setReadError(true);
        return true;
    }
    return false;
}

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

template <typename T>
bool
MqttObjectRegisterHolder<T>::isPolling() const {
    for(auto it = mRegisterValues.begin(); it != mRegisterValues.end(); it++)
        if (!it->second.isPolling()) {
            return false;
        }
    return true;
}

template <typename T>
ModbusRegisters
MqttObjectRegisterHolder<T>::getRawArray() const {
    ModbusRegisters ret;

    for(auto it = mRegisterValues.begin(); it != mRegisterValues.end(); it++)
        ret.addValue(it->second.getRawValue());

    return ret;
}

void
MqttObjectAvailability::addRegister(const MqttObjectRegisterIdent& regIdent, uint16_t availValue) {
    mRegisterValues[regIdent] = MqttObjectAvailabilityValue(availValue);
};

AvailableFlag
MqttObjectAvailability::getAvailableFlag() const {
    for(std::map<MqttObjectRegisterIdent, MqttObjectAvailabilityValue>::const_iterator it = mRegisterValues.begin(); it != mRegisterValues.end(); it++) {
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
    }
    return AvailableFlag::True;
}

void
MqttObjectState::addRegister(const std::string& name, const MqttObjectRegisterIdent& regIdent, const std::shared_ptr<IStateConverter>& conv) {
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
        case MqttValue::SourceType::DOUBLE:
            writer.Double(value.getDouble());
            break;
        case MqttValue::SourceType::BINARY:
            writer.String(static_cast<const char*>(value.getBinaryPtr()), value.getBinarySize());
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

void
createConvertedValue(
    rapidjson::Writer<rapidjson::StringBuffer>& writer,
    const ModbusRegisters& registers,
    const IStateConverter& converter
) {
    MqttValue v = converter.toMqtt(registers);
    createConvertedValue(writer, v);
}

void
createConvertedValue(
    rapidjson::Writer<rapidjson::StringBuffer>& writer,
    const MqttObjectStateValue& stateValue,
    const std::shared_ptr<IStateConverter>& converter
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
        if (single.isUnnamed() && single.isScalar()) {
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
                if (mConverter != nullptr)
                    createConvertedValue(writer, first.getRawArray(), *mConverter);
                else
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

bool
MqttObjectState::hasRegister(const MqttObjectRegisterIdent& regIdent) const {
    for(std::vector<MqttObjectStateValue>::const_iterator it = mValues.begin(); it != mValues.end(); it++) {
        if (it->hasRegister(regIdent))
            return true;
    }
    return false;
}


bool
MqttObjectState::updateRegisterValue(const MqttObjectRegisterIdent& ident, uint16_t value) {
    bool ret = false;
    for(std::vector<MqttObjectStateValue>::iterator it = mValues.begin(); it != mValues.end(); it++) {
        if (it->updateRegisterValue(ident, value))
            ret = true;
    }
    return ret;
}

bool
MqttObjectState::updateRegisterReadFailed(const MqttObjectRegisterIdent& regIdent) {
    bool ret = false;
    for(std::vector<MqttObjectStateValue>::iterator it = mValues.begin(); it != mValues.end(); it++) {
        if (it->updateRegisterReadFailed(regIdent))
            ret = true;
    }
    return ret;
}

bool
MqttObjectState::setModbusNetworkState(const std::string& networkName, bool isUp) {
    bool ret = false;
    for(std::vector<MqttObjectStateValue>::iterator it = mValues.begin(); it != mValues.end(); it++) {
        if (it->setModbusNetworkState(networkName, isUp))
            ret = true;
    }
    return ret;
}

bool
MqttObjectState::hasValues() const {
    for(std::vector<MqttObjectStateValue>::const_iterator it = mValues.begin(); it != mValues.end(); it++) {
        if (!it->hasValue())
            return false;
    }
    return true;
}

bool
MqttObjectState::isPolling() const {
    for(std::vector<MqttObjectStateValue>::const_iterator it = mValues.begin(); it != mValues.end(); it++) {
        if (!it->isPolling())
            return false;
    }
    return true;
}

MqttObject::MqttObject(const YAML::Node& data) {
    mTopic = ConfigTools::readRequiredString(data, "topic");
    mStateTopic = mTopic + "/state";
    mAvailabilityTopic = mTopic + "/availability";
};

void
MqttObject::updateRegisterValue(const MqttObjectRegisterIdent& regIdent, uint16_t value) {
    bool stateChanged = mState.updateRegisterValue(regIdent, value);
    bool availChanged = mAvailability.updateRegisterValue(regIdent, value);
    if (stateChanged || availChanged)
        updateAvailabilityFlag();
}

void
MqttObject::updateRegisterReadFailed(const MqttObjectRegisterIdent& regIdent) {
    bool stateChanged = mState.updateRegisterReadFailed(regIdent);
    bool availChanged = mAvailability.updateRegisterReadFailed(regIdent);
    if (stateChanged || availChanged)
        updateAvailabilityFlag();
}

void
MqttObject::setModbusNetworkState(const std::string& networkName, bool isUp) {
    bool stateChanged = mState.setModbusNetworkState(networkName, isUp);
    bool availChanged = mAvailability.setModbusNetworkState(networkName, isUp);
    if (stateChanged || availChanged)
        updateAvailabilityFlag();
}

void
MqttObject::updateAvailabilityFlag() {
    // if we cannot read availability registers
    // then assume that state data is invalid
    if (!mAvailability.isPolling() || !mState.isPolling())
        mIsAvailable = AvailableFlag::False;
    else {
        //we are reading all needed registers, check if
        //all of them have value
        if (!mAvailability.hasValue() || !mState.hasValues()) {
            mIsAvailable = AvailableFlag::NotSet;
        } else {
            //we have all values, check availability registers
            mIsAvailable = mAvailability.getAvailableFlag();
        }
    }
}

bool
MqttObject::hasCommand(const std::string& name) const {
    std::vector<MqttObjectCommand>::const_iterator it = std::find_if(
        mCommands.begin(), mCommands.end(),
        [&name](const MqttObjectCommand& item) -> bool { return item.mName == name; }
    );

    return it != mCommands.end();
}

} //namespace
