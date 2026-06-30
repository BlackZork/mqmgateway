#include <cstring>
#include <cassert>
#include <map>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "common.hpp"
#include "mqttclient.hpp"
#include "exceptions.hpp"
#include "modmqtt.hpp"
#include "default_command_converter.hpp"
#include "mqttpayload.hpp"

namespace modmqttd {

MqttClient::MqttClient(ModMqtt& modmqttd) : mOwner(modmqttd) {
    mMqttImpl.reset(new Mosquitto());
};

void
MqttClient::setBrokerConfig(const MqttBrokerConfig& config) {
    if (!mBrokerConfig.isSameAs(config)) {
        // TODO reconnect
        mBrokerConfig = config;
    }
};

void
MqttClient::setClientId(const std::string& clientId) {
    if (isStarted())
        throw MosquittoException("Cannot change client id when started");
    mRpcRequestTopic = clientId + "/rpc/modbus_request";
    mMqttImpl->init(this, clientId.c_str());
}

void
MqttClient::start() /*throw(MosquittoException)*/ {
    // protect from re-entry in ModMqtt main loop
    switch (mConnectionState) {
    case State::CONNECTED:
        return;
    }
    mIsStarted = true;
    mConnectionState = State::CONNECTING;
    mMqttImpl->connect(mBrokerConfig);
}

void
MqttClient::shutdown() {
    // do not add any messages to modbus queues - modbus clients
    // are already stopped
    mModbusClients.clear();

    switch (mConnectionState) {
    case State::CONNECTED:
        spdlog::info("Disconnecting from mqtt broker");
        mConnectionState = State::DISCONNECTING;
        mMqttImpl->disconnect();
        break;
    case State::CONNECTING:
        // we do not send disconnect mqtt request if not connected
        spdlog::info("Cancelling connection request");
        mIsStarted = false;
        break;
    case State::DISCONNECTING:
        spdlog::info("Shutdown already in progress, waiting for clean disconnect");
        break;
    default:
        mIsStarted = false;
        mConnectionState = State::DISCONNECTED;
    }
}

void
MqttClient::onDisconnect() {
    mPendingRpc.clear();
    for (std::vector<std::shared_ptr<ModbusClient>>::iterator it = mModbusClients.begin(); it != mModbusClients.end(); it++) {
        (*it)->sendMqttNetworkIsUp(false);
    }
    switch (mConnectionState) {
    case State::CONNECTED:
    case State::CONNECTING:
        spdlog::info("Reconnecting to mqtt broker");
        mMqttImpl->reconnect();
        break;
    case State::DISCONNECTING:
        spdlog::info("Stopping mosquitto message loop");
        mConnectionState = State::DISCONNECTED;
// https://github.com/BlackZork/mqmgateway/issues/33
#ifndef __MUSL__
        mMqttImpl->stop();
#endif
        mIsStarted = false;
        // signal modmqttd that is waiting on queues mutex
        // for us to disconnect
        modmqttd::notifyQueues();
    };
}

void
MqttClient::onConnect() {
    spdlog::info("Connected, sending subscriptions…");

    for (auto cmd: mCommands) {
        mMqttImpl->subscribe(cmd.second.mTopic.c_str());
    }
    if (mRpcMode != RpcMode::DISABLED) {
        mMqttImpl->subscribe(mRpcRequestTopic.c_str());
    }

    mConnectionState = State::CONNECTED;

    // if broker was restarted
    // then all published information is gone until
    // modbus register data is changed
    // republish current object state and availability to
    // all subscribed clients
    publishAll();

    for (std::vector<std::shared_ptr<ModbusClient>>::iterator it = mModbusClients.begin(); it != mModbusClients.end(); it++) {
        (*it)->sendMqttNetworkIsUp(true);
    }

    spdlog::info("Ready to process MQTT messages");
}

void
MqttClient::processRegisterValues(const std::string& pModbusNetworkName, const MsgRegisterValues& pSlaveData) {
    if (!isConnected()) {
        // we drop changes when there is no connection
        // retain flag is set so
        // broker will send last known value for us.
        spdlog::trace("Mqtt broker not connected, dropping MsgRegisterValues data");
        return;
    }

    std::vector<std::shared_ptr<MqttObject>>* affectedObjects = nullptr;

    if (pSlaveData.hasCommandId()) {
        MqttCmdObjMap::iterator it = mCommandObjects.find(pSlaveData.getCommandId());
        if (it != mCommandObjects.end()) {
            affectedObjects = &(it->second);
        } else if (pSlaveData.isRpc()) {
            publishRpcResponse(pModbusNetworkName, pSlaveData);
            // An RPC read also feeds the matching polled object, so state-topic
            // subscribers keep seeing values even while the scheduled poll is
            // deferred. The object's own publish logic decides whether to emit:
            // ON_CHANGE publishes only on a real value change (no flood, every
            // change captured), EVERY_POLL is rate-limited to the refresh period
            // (a burst of RPC reads cannot flood the topic). The RPC register
            // need not be polled - a lookup miss just means no object to update.
            MqttObjectRegisterIdent ident(pModbusNetworkName, pSlaveData);
            MqttPollObjMap::iterator oit = mObjects.find(ident);
            if (oit != mObjects.end()) {
                affectedObjects = &(oit->second);
            }
        }
    } else {
        MqttObjectRegisterIdent ident(pModbusNetworkName, pSlaveData);
        MqttPollObjMap::iterator it = mObjects.find(ident);
        assert(it != mObjects.end());
        affectedObjects = &(it->second);
    }

    // possible if write command registers do not overlap with
    // any MqttObject
    if (affectedObjects == nullptr) {
        spdlog::trace("No affected objects for received register values");
        return;
    }

    try {
        for (std::shared_ptr<MqttObject>& obj: *affectedObjects) {
            AvailableFlag oldAvail = obj->getAvailableFlag();
            obj->updateRegisterValues(pModbusNetworkName, pSlaveData);
            AvailableFlag newAvail = obj->getAvailableFlag();

            if (oldAvail != newAvail) {
                if (newAvail == AvailableFlag::True) {
                    // if object is not retained
                    // then publish state changes only
                    // if availability is already set to true
                    if (obj->getRetain()) {
                        publishState(obj, true);
                    } else {
                        // delete retained message
                        if (oldAvail == AvailableFlag::NotSet) {
                            mMqttImpl->publish(obj->getStateTopic().c_str(), 0, NULL, true);
                            // remember initial payload for comparsion with subsequent modbus data updates
                            if (!obj->getRetain())
                                obj->setLastPublishedPayload(MqttPayload::generate(*obj));
                        }
                        if (obj->getPublishMode() == PublishMode::EVERY_POLL)
                            publishState(obj, true);
                    }
                }
                publishAvailabilityChange(*obj);
            } else {
                publishState(obj, obj->needStateRepublish());
            }
        }
    } catch (const MosquittoException& ex) {
        spdlog::error("Failed to publish mqtt state message: {}", ex.what());
    }
}

void
MqttClient::publishState(const std::shared_ptr<MqttObject>& obj, bool force) {
    if (obj->getAvailableFlag() != AvailableFlag::True)
        return;
    if (obj->getPublishMode() == PublishMode::ONCE) {
        if (obj->getLastPublishTime() != std::chrono::steady_clock::time_point::min())
            return;
    }

    std::string messageData(MqttPayload::generate(*obj));
    if (messageData != obj->getLastPublishedPayload() || force) {
        int msgId = mMqttImpl->publish(obj->getStateTopic().c_str(), messageData.length(), messageData.c_str(), obj->getRetain());
        spdlog::debug("Publish {} on topic {}: {}", msgId, obj->getStateTopic(), messageData);
        obj->setLastPublishedPayload(messageData);
        obj->setLastPublishTime(std::chrono::steady_clock::now());
    }
}

void
MqttClient::processRegistersOperationFailed(const std::string& pModbusNetworkName, const ModbusMessageBase& pSlaveData) {
    MqttObjectRegisterIdent ident(pModbusNetworkName, pSlaveData);
    MqttPollObjMap::iterator it = mObjects.find(ident);

    // msg was sent after failed write and
    // is not related MqttObjectState
    if (it == mObjects.end())
        return;

    for (std::shared_ptr<MqttObject>& obj: it->second) {
        AvailableFlag oldAvail = obj->getAvailableFlag();
        obj->updateRegistersReadFailed(pModbusNetworkName, pSlaveData);
        AvailableFlag newAvail = obj->getAvailableFlag();

        publishState(obj);

        if (oldAvail != newAvail) {
            publishAvailabilityChange(*obj);
        }
    }
}

void
MqttClient::processModbusNetworkState(const std::string& pNetworkName, bool pIsUp) {
    // wait for initial poll
    if (pIsUp)
        return;

    for (MqttPollObjMap::iterator it = mObjects.begin(); it != mObjects.end(); it++) {
        if (it->first.mNetworkName != pNetworkName)
            continue;

        for (std::vector<std::shared_ptr<MqttObject>>::iterator oit = it->second.begin(); oit != it->second.end(); oit++) {
            std::set<std::shared_ptr<MqttObject>> processed;
            const std::shared_ptr<MqttObject>& optr = *oit;
            if (processed.find(optr) == processed.end()) {

                AvailableFlag oldAvail = (*oit)->getAvailableFlag();
                (*oit)->setModbusNetworkState(pNetworkName, pIsUp);
                if (oldAvail != (*oit)->getAvailableFlag())
                    publishAvailabilityChange(**oit);
                processed.insert(optr);
            }
        }
    }
}

void
MqttClient::publishAvailabilityChange(const MqttObject& obj) {
    if (obj.getAvailableFlag() == AvailableFlag::NotSet)
        return;
    try {
        char msg = obj.getAvailableFlag() == AvailableFlag::True ? '1' : '0';
        mMqttImpl->publish(obj.getAvailabilityTopic().c_str(), 1, &msg, true);
        spdlog::debug("Publish on topic {}: {}", obj.getAvailabilityTopic(), msg);
    } catch (const MosquittoException& ex) {
        spdlog::error("Failed to publish mqtt availability message: {}", ex.what());
    }
}

void
MqttClient::publishAll() {
    std::set<std::shared_ptr<MqttObject>> published;

    for (MqttPollObjMap::iterator it = mObjects.begin(); it != mObjects.end(); it++) {
        for (std::vector<std::shared_ptr<MqttObject>>::iterator oit = it->second.begin(); oit != it->second.end(); oit++) {
            const std::shared_ptr<MqttObject>& optr = *oit;

            if (published.find(optr) == published.end()) {
                if ((*oit)->getAvailableFlag() == AvailableFlag::True)
                    publishState(*oit, true);
                publishAvailabilityChange(**oit);
                published.insert(*oit);
            }
        }
    }
}

MqttValue
createMqttValue(const MqttObjectCommand& command, const void* data, int datalen) {
    MqttValue ret;

    switch (command.mPayloadType) {
    case MqttObjectCommand::PayloadType::STRING:
        ret = MqttValue::fromBinary(data, datalen);
        break;
    default:
        throw MqttPayloadConversionException("Conversion failed, unknown payload type" + std::to_string(command.mPayloadType));
    }

    return ret;
}

// Build an MqttValue from an RPC write's JSON "value" field for converter encoding.
// Mirrors how a configured command feeds its payload to converter->toModbus().
static MqttValue
rpcWriteValueFromJson(const rapidjson::Value& pValue) {
    if (pValue.IsString()) {
        return MqttValue::fromString(pValue.GetString());
    }
    if (pValue.IsDouble()) {
        return MqttValue::fromDouble(pValue.GetDouble());
    }
    if (pValue.IsInt()) {
        return MqttValue::fromInt(pValue.GetInt());
    }
    if (pValue.IsInt64()) {
        return MqttValue::fromInt64(pValue.GetInt64());
    }
    if (pValue.IsUint()) {
        return MqttValue::fromInt64(pValue.GetUint());
    }
    if (pValue.IsUint64()) {
        // TODO MqttValue has no unsigned 64-bit holder; reject values that do not fit int64
        const uint64_t u = pValue.GetUint64();
        if (u > static_cast<uint64_t>(INT64_MAX)) {
            throw std::invalid_argument("value out of range");
        }
        return MqttValue::fromInt64(static_cast<int64_t>(u));
    }
    throw std::invalid_argument("converter write requires a scalar value (number or string)");
}

void
MqttClient::onMessage(const char* pTopic, const void* pPayload, int pPayloadlen,
                      const char* pResponseTopic,
                      const std::shared_ptr<void>& pCorrelationData, int pCorrelationLen) {
    auto cmd = findCommand(pTopic);
    if (cmd == mCommands.end()) {
        if (mRpcMode != RpcMode::DISABLED && mRpcRequestTopic == pTopic) {
            handleRpcRequest(pPayload, pPayloadlen, pResponseTopic, pCorrelationData, pCorrelationLen);
        } else {
            spdlog::error("No command for topic {}, dropping message", pTopic);
        }
        return;
    }
    try {
        const MqttObjectCommand& command = cmd->second;
        const std::string& network = command.mModbusNetworkName;

        // TODO is is thread safe to iterate on modbus clients from mosquitto callback?
        std::vector<std::shared_ptr<ModbusClient>>::const_iterator it = std::find_if(
            mModbusClients.begin(), mModbusClients.end(),
            [&network](const std::shared_ptr<ModbusClient>& pClient) -> bool { return pClient->mNetworkName == network; });
        if (it == mModbusClients.end()) {
            spdlog::error("Modbus network {} not found for command {}, dropping message", network, pTopic);
        } else {
            MqttValue tmpval(createMqttValue(command, pPayload, pPayloadlen));

            ModbusRegisters reg_values;

            if (command.hasConverter()) {
                reg_values = command.getConverter().toModbus(tmpval, command.mCount);
            } else {
                reg_values = mDefaultConverter.toModbus(tmpval, command.mCount);
            }

            if (reg_values.getCount() != command.mCount) {
                throw MqttPayloadConversionException(std::string("Conversion failed, expecting ") + std::to_string(command.mCount) + " register values, got " + std::to_string(reg_values.getCount()));
            }

            (*it)->sendCommand(command, reg_values);
        }
    } catch (const ConvException& ex) {
        spdlog::error("Converter error for {}: {}", pTopic, ex.what());
    } catch (const MqttPayloadConversionException& ex) {
        spdlog::error("Value error for {}: {}", pTopic, ex.what());
    }
}

void
MqttClient::addCommand(const MqttObjectCommand& pCommand) {
    mCommands.insert(std::pair<std::string, MqttObjectCommand>(pCommand.mTopic, pCommand));
}


std::map<std::string, MqttObjectCommand>::const_iterator
MqttClient::findCommand(const char* topic) const {
    return mCommands.find(topic);
}

void
MqttClient::handleRpcRequest(const void* pPayload, int pPayloadlen,
                             const char* pResponseTopic,
                             const std::shared_ptr<void>& pCorrelationData, int pCorrelationLen) {
    if (pResponseTopic == nullptr || pResponseTopic[0] == '\0') {
        spdlog::error("RPC request without response topic, dropping");
        return;
    }

    const std::string respTopic(pResponseTopic);
    const CorrelationData corrData(pCorrelationData, pCorrelationLen);
    spdlog::trace("RPC request received: responseTopic={}, payloadLen={}", respTopic, pPayloadlen);

    try {
        rapidjson::Document doc;
        const char* src = static_cast<const char*>(pPayload);
        doc.Parse(src, pPayloadlen);
        if (doc.HasParseError()) {
            size_t off = doc.GetErrorOffset();
            int line = 1, col = 1;
            for (size_t i = 0; i < off; ++i) {
                if (src[i] == '\n') {
                    ++line;
                    col = 1;
                } else {
                    ++col;
                }
            }
            throw std::invalid_argument(
                std::string("JSON parse error at ") + std::to_string(line) + ":" + std::to_string(col) + ": " + rapidjson::GetParseError_En(doc.GetParseError()));
        }
        if (!doc.IsObject()) {
            throw std::invalid_argument("object expected");
        }

        if (!doc.HasMember("network") || !doc["network"].IsString()) {
            throw std::invalid_argument("missing or invalid field: network");
        }
        const std::string networkName(doc["network"].GetString());

        if (!doc.HasMember("slave") || !doc["slave"].IsInt()) {
            throw std::invalid_argument("missing or invalid field: slave");
        }
        int slaveId = doc["slave"].GetInt();

        if (!doc.HasMember("register") || !doc["register"].IsString()) {
            throw std::invalid_argument("missing or invalid field: register");
        }
        const std::string regStr(doc["register"].GetString());
        int regNum = ConfigTools::registerNumberFromString(regStr);

        RegisterType regType = RegisterType::HOLDING;
        if (doc.HasMember("register_type")) {
            if (!doc["register_type"].IsString()) {
                throw std::invalid_argument("invalid field: register_type");
            }
            const std::string rtStr(doc["register_type"].GetString());
            if (rtStr == "coil") {
                regType = RegisterType::COIL;
            } else if (rtStr == "input") {
                regType = RegisterType::INPUT;
            } else if (rtStr == "bit") {
                regType = RegisterType::BIT;
            } else if (rtStr != "holding") {
                throw std::invalid_argument("unknown register_type: " + rtStr);
            }
        }

        std::string converterSpec;
        std::shared_ptr<DataConverter> converter; // null => raw register value(s)
        if (doc.HasMember("converter")) {
            if (!doc["converter"].IsString()) {
                throw std::invalid_argument("invalid field: converter");
            }
            converterSpec = doc["converter"].GetString();
            if (!converterSpec.empty() && converterSpec != "none") {
                converter = mOwner.createConverterFromString(converterSpec);
            }
        }

        std::vector<std::shared_ptr<ModbusClient>>::iterator netIt = std::find_if(
            mModbusClients.begin(), mModbusClients.end(),
            [&networkName](const std::shared_ptr<ModbusClient>& pC) { return pC->mNetworkName == networkName; });
        if (netIt == mModbusClients.end()) {
            throw std::invalid_argument("network not found: " + networkName);
        }

        const bool isWrite = doc.HasMember("value");
        if (isWrite && mRpcMode != RpcMode::READ_WRITE) {
            throw std::invalid_argument("writes are disabled (mode: read)");
        }
        if (isWrite && (regType == RegisterType::BIT || regType == RegisterType::INPUT)) {
            throw std::invalid_argument("register_type is read-only");
        }

        // per-type libmodbus limits: 125 for holding/input registers, 2000 for coils/bits
        const int maxCount = (regType == RegisterType::COIL || regType == RegisterType::BIT) ? 2000 : 125;

        int count = 1;
        std::vector<uint16_t> writeValues;
        if (isWrite && converter == nullptr) {
            // raw write: register count is derived from the value
            const rapidjson::Value& val = doc["value"];
            if (val.IsUint() && val.GetUint() <= UINT16_MAX) {
                writeValues.push_back(static_cast<uint16_t>(val.GetUint()));
            } else if (val.IsArray()) {
                writeValues.reserve(val.Size());
                for (rapidjson::SizeType i = 0; i < val.Size(); i++) {
                    if (!val[i].IsUint() || val[i].GetUint() > UINT16_MAX) {
                        throw std::invalid_argument("value elements must be uint16 integers");
                    }
                    writeValues.push_back(static_cast<uint16_t>(val[i].GetUint()));
                }
            } else {
                throw std::invalid_argument("value must be a uint16 integer or an array of uint16 values");
            }
            count = static_cast<int>(writeValues.size());
        } else {
            // read, or converter-encoded write: honour the optional "count" field
            // (a converter needs the register count as an explicit input)
            if (doc.HasMember("count")) {
                if (!doc["count"].IsInt()) {
                    throw std::invalid_argument("invalid field: count");
                }
                count = doc["count"].GetInt();
                if (count < 1 || count > maxCount) {
                    throw std::invalid_argument(
                        std::string("count out of range [1, ") + std::to_string(maxCount) + "]");
                }
            }
            if (isWrite) {
                ModbusRegisters regs = converter->toModbus(rpcWriteValueFromJson(doc["value"]), count);
                if (regs.getCount() != count) {
                    throw std::invalid_argument(
                        std::string("converter produced ") + std::to_string(regs.getCount()) + " register(s), expected " + std::to_string(count));
                }
                writeValues = regs.values();
            }
        }

        // clang-format off
        spdlog::trace("RPC request parsed: {} network={} slave={} register={} type={} count={} converter={}",
            isWrite ? "write" : "read", networkName, slaveId, regStr,
            regType == RegisterType::COIL ? "coil" : regType == RegisterType::INPUT ? "input" : regType == RegisterType::BIT ? "bit" : "holding",
            count, converter != nullptr ? converterSpec : "none");
        // clang-format on

        if (mNextRpcId == INT_MIN) {
            mNextRpcId = 0;
        }
        const int id = --mNextRpcId;

        PendingRpcRequest pending;
        pending.mNetworkName = networkName;
        pending.mResponseTopic = respTopic;
        pending.mCorrelationData = corrData;
        pending.mSlaveId = slaveId;
        pending.mRegisterNumber = regNum;
        pending.mDisplayAddress = regStr;
        pending.mRegisterType = regType;
        pending.mCount = count;
        pending.mIsWrite = isWrite;
        pending.mConverter = converter;
        mPendingRpc[id] = std::move(pending);

        if (isWrite) {
            MsgRegisterValues msg(slaveId, regType, regNum, ModbusRegisters(writeValues), id, ModbusWriteMode::AUTO);
            (*netIt)->sendWriteRequest(msg);
        } else {
            (*netIt)->sendReadRequest(MsgRegisterReadRequest(slaveId, regType, regNum, count, id));
        }
        spdlog::trace("RPC request enqueued: commandId={} ({})", id, isWrite ? "write" : "read");
    } catch (const std::exception& ex) {
        spdlog::error("RPC request failed: {}", ex.what());
        publishRpcError(respTopic, corrData, ex.what());
    }
}

void
MqttClient::publishRpcResponse(const std::string& pNetworkName, const MsgRegisterValues& pValues) {
    MqttRpcPendingMap::iterator it = mPendingRpc.find(pValues.getCommandId());
    if (it == mPendingRpc.end()) {
        spdlog::error("RPC response for unknown commandId {}", pValues.getCommandId());
        return;
    }
    PendingRpcRequest pending = std::move(it->second);
    mPendingRpc.erase(it);
    spdlog::trace("Got modbus data for RPC response, commandId={} ({} {}.{})",
                  pValues.getCommandId(), pending.mIsWrite ? "write" : "read",
                  pending.mSlaveId, pending.mDisplayAddress);

    std::string payload;
    try {
        if (!pending.mIsWrite && pending.mConverter != nullptr) {
            // read with a converter: converter output is the payload, exactly as a poll would publish it
            payload = pending.mConverter->toMqtt(pValues.mRegisters).getString();
        } else if (pValues.mRegisters.getCount() == 1) {
            // bare raw value: scalar string for count==1, JSON array for count>1
            // (same shape as MqttPayload::generate for a plain poll with no converter)
            // Used by reads without a converter and by all write replies — the optimistic
            // echo of the registers just written (no converter re-applied, no device re-read).
            payload = std::to_string(pValues.mRegisters.getValue(0));
        } else {
            rapidjson::StringBuffer buf;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
            writer.StartArray();
            for (int i = 0; i < pValues.mRegisters.getCount(); i++) {
                writer.Uint(pValues.mRegisters.getValue(i));
            }
            writer.EndArray();
            payload = buf.GetString();
        }
    } catch (const std::exception& ex) {
        spdlog::error("RPC response serialization failed: {}", ex.what());
        publishRpcError(pending.mResponseTopic, pending.mCorrelationData, "internal error");
        return;
    }

    // success: scalar or JSON-array payload for reads/writes, empty only on error
    try {
        mMqttImpl->publishResponse(
            pending.mResponseTopic.c_str(),
            static_cast<int>(payload.size()),
            payload.empty() ? nullptr : payload.c_str(),
            pending.mCorrelationData.data(),
            pending.mCorrelationData.size());
        spdlog::debug(
            "RPC {} {}.{} ok: count={}, responseTopic={}, payloadLen={}, payload={}",
            pending.mIsWrite ? "write" : "read",
            pending.mSlaveId,
            pending.mDisplayAddress,
            pending.mCount,
            pending.mResponseTopic,
            payload.size(),
            payload);
    } catch (const std::exception& ex) {
        spdlog::error("Failed to publish RPC response: {}", ex.what());
    }
}

void
MqttClient::publishRpcError(const std::string& pResponseTopic,
                            const CorrelationData& pCorrelationData, const std::string& pErrorMsg) {
    spdlog::trace("Publishing RPC error to {}: {}", pResponseTopic, pErrorMsg);
    try {
        // empty payload + single "error" user property (MQTT5 User Property channel)
        mMqttImpl->publishResponse(
            pResponseTopic.c_str(),
            0, nullptr,
            pCorrelationData.data(),
            pCorrelationData.size(),
            {{"error", pErrorMsg}});
    } catch (const std::exception& ex) {
        spdlog::error("Failed to publish RPC error: {}", ex.what());
    }
}

void
MqttClient::publishRpcError(const std::string& pModbusNetworkName, const ModbusMessageBase& pSlaveData) {
    MqttRpcPendingMap::iterator it = mPendingRpc.find(pSlaveData.getCommandId());
    if (it == mPendingRpc.end()) {
        spdlog::error("RPC error for unknown commandId {}", pSlaveData.getCommandId());
        return;
    }
    PendingRpcRequest pending = std::move(it->second);
    mPendingRpc.erase(it);
    const std::string errorMsg = pending.mIsWrite ? "modbus write failed" : "modbus read failed";
    spdlog::trace("RPC modbus {} failed: commandId={}, slave={}.{}",
                  pending.mIsWrite ? "write" : "read", pSlaveData.getCommandId(),
                  pending.mSlaveId, pending.mDisplayAddress);
    publishRpcError(pending.mResponseTopic, pending.mCorrelationData, errorMsg);
}
}
