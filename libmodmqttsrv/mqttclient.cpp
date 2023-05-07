#include <cstring>

#include "mqttclient.hpp"
#include "exceptions.hpp"
#include "modmqtt.hpp"

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
    switch(mConnectionState) {
        case State::CONNECTED:
            BOOST_LOG_SEV(log, Log::info) << "Disconnecting from mqtt broker";
            mConnectionState = State::DISCONNECTING;
            mMqttImpl->disconnect();
        break;
        case State::CONNECTING:
            //we do not send disconnect mqtt request if not connected
            BOOST_LOG_SEV(log, Log::info) << "Cancelling connection request";
            mIsStarted = false;
            break;
        case State::DISCONNECTING:
            BOOST_LOG_SEV(log, Log::info) << "Shutdown already in progress, waiting for clean disconnect";
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
            BOOST_LOG_SEV(log, Log::info) << "reconnecting to mqtt broker";
            mMqttImpl->reconnect();
            break;
        case State::DISCONNECTING:
            BOOST_LOG_SEV(log, Log::info) << "Stopping mosquitto message loop";
            mConnectionState = State::DISCONNECTED;
            mMqttImpl->stop();
            mIsStarted = false;
            // signal modmqttd that is waiting on queues mutex
            // for us to disconnect
            modmqttd::notifyQueues();
    };
}

void
MqttClient::onConnect() {
	BOOST_LOG_SEV(log, Log::info) << "Mqtt conected, sending subscriptions...";

    for(std::vector<MqttObject>::const_iterator obj = mObjects.begin(); obj != mObjects.end(); obj++) {
        for(auto it = obj->mCommands.begin(); it != obj->mCommands.end(); it++)
            subscribeToCommandTopic(obj->getTopic(), *it);
    }

    mConnectionState = State::CONNECTED;

    // if broker was restarted
    // then all published information is gone until
    // modbus register data is changed
    // republish current object state and availability to
    // for subscribed clients
    publishAll();

    for(std::vector<std::shared_ptr<ModbusClient>>::iterator it = mModbusClients.begin(); it != mModbusClients.end(); it++) {
        (*it)->sendMqttNetworkIsUp(true);
    }

	BOOST_LOG_SEV(log, Log::info) << "Mqtt ready to process messages";
}

void
MqttClient::subscribeToCommandTopic(const std::string& objectTopic, const MqttObjectBase& cmd) {
    std::string topic = objectTopic + "/" + cmd.mName;
    mMqttImpl->subscribe(topic.c_str());
}

void
MqttClient::processRegisterValue(const MqttObjectRegisterIdent& ident, uint16_t value) {
    if (!isConnected()) {
        // we drop changes when there is no connection
        // retain flag is set so
        // broker will send last known value for us.
        return;
    }

    for(std::vector<MqttObject>::iterator it = mObjects.begin();
        it != mObjects.end(); it++)
    {
        AvailableFlag oldAvail = it->getAvailableFlag();
        it->updateRegisterValue(ident, value);
        AvailableFlag newAvail = it->getAvailableFlag();

        if (it->mState.hasRegister(ident) && it->mState.hasValues()) {
            publishState(*it);
        }

        if (oldAvail != newAvail) {
            publishAvailabilityChange(*it);
        }
    }
}

void
MqttClient::publishState(const MqttObject& obj) {
    int msgId;
    std::string messageData(obj.mState.createMessage());
    BOOST_LOG_SEV(log, Log::debug) << "Publish on topic " << obj.getStateTopic() << ": " << messageData;
    mMqttImpl->publish(obj.getStateTopic().c_str(), messageData.length(), messageData.c_str());
}

void
MqttClient::processRegisterOperationFailed(const MqttObjectRegisterIdent& ident) {
    for(std::vector<MqttObject>::iterator it = mObjects.begin();
        it != mObjects.end(); it++)
    {
        AvailableFlag oldAvail = it->getAvailableFlag();
        it->updateRegisterReadFailed(ident);
        if (oldAvail != it->getAvailableFlag())
            publishAvailabilityChange(*it);
    }
}

void
MqttClient::processModbusNetworkState(const std::string& networkName, bool isUp) {
    for(std::vector<MqttObject>::iterator it = mObjects.begin();
        it != mObjects.end(); it++)
    {
        AvailableFlag oldAvail = it->getAvailableFlag();
        it->setModbusNetworkState(networkName, isUp);
        if (oldAvail != it->getAvailableFlag())
            publishAvailabilityChange(*it);
    }
}

void
MqttClient::publishAvailabilityChange(const MqttObject& obj) {
    if (obj.getAvailableFlag() == AvailableFlag::NotSet)
            return;
    char msg = obj.getAvailableFlag() == AvailableFlag::True ? '1' : '0';
    int msgId;
    mMqttImpl->publish(obj.getAvailabilityTopic().c_str(), 1, &msg);
}

void
MqttClient::publishAll() {
    for(auto object = mObjects.begin(); object != mObjects.end(); object++) {
        if (object->getAvailableFlag() == AvailableFlag::True)
            publishState(*object);
        publishAvailabilityChange(*object);
    }
}

static const int MAX_DATA_LEN = 32;

uint16_t
convertMqttPayload(const MqttObjectBase& command, const void* data, int datalen) {
    uint16_t ret(0);
    if (datalen > MAX_DATA_LEN)
        throw MqttPayloadConversionException(std::string("Conversion failed, payload too big (size:") + std::to_string(datalen) + ")");

    switch(command.mPayloadType) {
        case MqttObjectCommand::PayloadType::STRING: {
            std::string value((const char*)data, datalen);
            try {
                int temp(std::stoi(value.c_str()));
                if (temp <= static_cast<int>(UINT16_MAX) && temp >=0) {
                    ret = static_cast<uint16_t>(temp);
                } else {
                    throw MqttPayloadConversionException(std::string("Conversion failed, value " + std::to_string(temp) + " out of range"));
                }
            } catch (const std::invalid_argument& ex) {
                throw MqttPayloadConversionException("Failed to convert mqtt value to int16");
            } catch (const std::out_of_range& ex) {
                throw MqttPayloadConversionException("mqtt value if out of range");
            }
        } break;
        default:
          throw MqttPayloadConversionException("Conversion failed, unknown payload type" + std::to_string(command.mPayloadType));
    }

    switch(command.mRegister.mRegisterType) {
        case RegisterType::BIT:
        case RegisterType::COIL:
            if ((ret != 0) && (ret != 1)) {
                throw MqttPayloadConversionException(std::string("Conversion failed, cannot convert " + std::to_string(ret) + " to single bit"));
            }
        break;
    }
    return ret;
}

void
MqttClient::onMessage(const char* topic, const void* payload, int payloadlen) {
    try {
        const MqttObjectBase& command = findCommand(topic);
        const std::string network = command.mRegister.mNetworkName;

        //TODO is is thread safe to iterate on modbus clients from mosquitto callback?
        std::vector<std::shared_ptr<ModbusClient>>::const_iterator it = std::find_if(
            mModbusClients.begin(), mModbusClients.end(),
            [&network](const std::shared_ptr<ModbusClient>& client) -> bool { return client->mName == network; }
        );
        if (it == mModbusClients.end()) {
            BOOST_LOG_SEV(log, Log::error) << "Modbus network " << network << " not found for command  " << topic << ", dropping message";
        } else {
            uint16_t value = convertMqttPayload(command, payload, payloadlen);
            (*it)->sendCommand(command, value);
        }
    } catch (const MqttPayloadConversionException& ex) {
        BOOST_LOG_SEV(log, Log::error) << "Value error for " << topic << ":" << ex.what();
    } catch (const ObjectCommandNotFoundException&) {
        BOOST_LOG_SEV(log, Log::error) << "No command for topic " << topic << ", dropping message";
    }
}

const MqttObjectBase&
MqttClient::findCommand(const char* topic) const {
    std::string objectName;
    std::string commandName;
    const char *ptr = strrchr(topic, '/');
    if(ptr) {
       objectName = std::string(topic, ptr);
       commandName = std::string(ptr+1);
    } else {
        //should never happen because we force commands to have a name
        objectName = topic;
    }


    std::vector<MqttObject>::const_iterator obj = std::find_if(
        mObjects.begin(), mObjects.end(),
        [&objectName](const MqttObject& item) -> bool { return item.getTopic() == objectName; }
    );
    if (obj != mObjects.end()) {
        std::vector<MqttObjectCommand>::const_iterator cmd = std::find_if(
            obj->mCommands.begin(), obj->mCommands.end(),
            [&commandName](const MqttObjectCommand& item) -> bool { return item.mName == commandName; }
        );
        if (cmd != obj->mCommands.end())
            return *cmd;
        }
    throw ObjectCommandNotFoundException(topic);
}

}
