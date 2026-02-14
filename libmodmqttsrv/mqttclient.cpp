#include <cstring>
#include <cassert>
#include <map>

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
		//TODO reconnect
        mBrokerConfig = config;
    }
};

void
MqttClient::setClientId(const std::string& clientId) {
    if (isStarted())
        throw MosquittoException("Cannot change client id when started");
    mMqttImpl->init(this, clientId.c_str());
}

void
MqttClient::start() /*throw(MosquittoException)*/ {
    // protect from re-entry in ModMqtt main loop
	switch(mConnectionState) {
		case State::CONNECTED:
			return;
	}
    mIsStarted = true;
    mConnectionState = State::CONNECTING;
    mMqttImpl->connect(mBrokerConfig);
}

void
MqttClient::shutdown() {
    //do not add any messages to modbus queues - modbus clients
    //are already stopped
    mModbusClients.clear();

    switch(mConnectionState) {
        case State::CONNECTED:
            spdlog::info("Disconnecting from mqtt broker");
            mConnectionState = State::DISCONNECTING;
            mMqttImpl->disconnect();
        break;
        case State::CONNECTING:
            //we do not send disconnect mqtt request if not connected
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
    for(std::vector<std::shared_ptr<ModbusClient>>::iterator it = mModbusClients.begin(); it != mModbusClients.end(); it++) {
        (*it)->sendMqttNetworkIsUp(false);
    }
    switch(mConnectionState) {
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

    for(auto cmd: mCommands) {
        mMqttImpl->subscribe(cmd.second.mTopic.c_str());
    }

    mConnectionState = State::CONNECTED;

    // if broker was restarted
    // then all published information is gone until
    // modbus register data is changed
    // republish current object state and availability to
    // all subscribed clients
    publishAll();

    for(std::vector<std::shared_ptr<ModbusClient>>::iterator it = mModbusClients.begin(); it != mModbusClients.end(); it++) {
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
        if (it != mCommandObjects.end())
            affectedObjects = &(it->second);
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
                    publishState(*obj, true);
                } else {
                    // delete retained message
                    if (oldAvail == AvailableFlag::NotSet) {
                        mMqttImpl->publish(obj->getStateTopic().c_str(), 0, NULL, true);
                        // remember initial payload for comparsion with subsequent modbus data updates
                        if (!obj->getRetain())
                            obj->setLastPublishedPayload(MqttPayload::generate(*obj));
                    }
                    if (obj->getPublishMode() == PublishMode::EVERY_POLL)
                        publishState(*obj, true);
                }
            }

            publishAvailabilityChange(*obj);
        } else {
            publishState(*obj, obj->needStateRepublish());
        }
    }
}

void
MqttClient::publishState(MqttObject& obj, bool force) {
    if (obj.getAvailableFlag() != AvailableFlag::True)
        return;
    std::string messageData(MqttPayload::generate(obj));
    if (messageData != obj.getLastPublishedPayload() || force) {
        spdlog::debug("Publish on topic {}: {}", obj.getStateTopic(), messageData);
        mMqttImpl->publish(obj.getStateTopic().c_str(), messageData.length(), messageData.c_str(), obj.getRetain());
        obj.setLastPublishedPayload(messageData);
    }
}

void
MqttClient::processRegistersOperationFailed(const std::string& pModbusNetworkName, const ModbusSlaveAddressRange& pSlaveData) {
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

        publishState(*obj);

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

    for(MqttPollObjMap::iterator it = mObjects.begin(); it != mObjects.end(); it++)
    {
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
    char msg = obj.getAvailableFlag() == AvailableFlag::True ? '1' : '0';
    spdlog::debug("Publish on topic {}: {}", obj.getAvailabilityTopic(), msg);
    mMqttImpl->publish(obj.getAvailabilityTopic().c_str(), 1, &msg, true);
}

void
MqttClient::publishAll() {
    std::set<std::shared_ptr<MqttObject>> published;

    for(MqttPollObjMap::iterator it = mObjects.begin(); it != mObjects.end(); it++)
    {
        for (std::vector<std::shared_ptr<MqttObject>>::iterator oit = it->second.begin(); oit != it->second.end(); oit++) {
            const std::shared_ptr<MqttObject>& optr = *oit;
            if (published.find(optr) == published.end()) {
                if ((*oit)->getAvailableFlag() == AvailableFlag::True)
                    publishState(**oit, true);
                publishAvailabilityChange(**oit);
                published.insert(*oit);
            }
        }
    }
}

MqttValue
createMqttValue(const MqttObjectCommand& command, const void* data, int datalen) {
    MqttValue ret;

    switch(command.mPayloadType) {
        case MqttObjectCommand::PayloadType::STRING:
            ret = MqttValue::fromBinary(data, datalen);
        break;
        default:
          throw MqttPayloadConversionException("Conversion failed, unknown payload type" + std::to_string(command.mPayloadType));
    }

    return ret;
}

void
MqttClient::onMessage(const char* topic, const void* payload, int payloadlen) {
    try {
        const MqttObjectCommand& command = findCommand(topic);
        const std::string network = command.mModbusNetworkName;

        //TODO is is thread safe to iterate on modbus clients from mosquitto callback?
        std::vector<std::shared_ptr<ModbusClient>>::const_iterator it = std::find_if(
            mModbusClients.begin(), mModbusClients.end(),
            [&network](const std::shared_ptr<ModbusClient>& client) -> bool { return client->mNetworkName == network; }
        );
        if (it == mModbusClients.end()) {
            spdlog::error("Modbus network {} not found for command {}, dropping message", network, topic);
        } else {
            MqttValue tmpval(createMqttValue(command, payload, payloadlen));

            ModbusRegisters reg_values;

            if (command.hasConverter()) {
                reg_values = command.getConverter().toModbus(tmpval, command.mCount);
            } else {
                reg_values = mDefaultConverter.toModbus(tmpval, command.mCount);
            }

            if (reg_values.getCount() != command.mCount)
                throw MqttPayloadConversionException(std::string("Conversion failed, expecting ") + std::to_string(command.mCount) + " register values, got " + std::to_string(reg_values.getCount()));

            (*it)->sendCommand(command, reg_values);
        }
    } catch (const ConvException& ex) {
        spdlog::error("Converter error for {}: {}", topic, ex.what());
    } catch (const MqttPayloadConversionException& ex) {
        spdlog::error("Value error for {}: {}", topic, ex.what());
    } catch (const ObjectCommandNotFoundException&) {
        spdlog::error("No command for topic {}, dropping message", topic);
    }
}

void
MqttClient::addCommand(const MqttObjectCommand& pCommand) {
    mCommands.insert(std::pair<std::string, MqttObjectCommand>(pCommand.mTopic, pCommand));
}


const MqttObjectCommand&
MqttClient::findCommand(const char* topic) const {
    auto cmd = mCommands.find(topic);
    if (cmd != mCommands.end())
        return cmd->second;
    throw ObjectCommandNotFoundException(topic);
}

}
