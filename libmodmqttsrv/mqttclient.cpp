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
	BOOST_LOG_SEV(log, Log::info) << "Mqtt connected, sending subscriptions...";

    for(std::vector<MqttObject>::const_iterator obj = mObjects.begin(); obj != mObjects.end(); obj++) {
        for(auto it = obj->mCommands.begin(); it != obj->mCommands.end(); it++)
            subscribeToCommandTopic(obj->getTopic(), *it);
        for(auto it = obj->mRemoteCalls.begin(); it != obj->mRemoteCalls.end(); it++)
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

/**
 * Make a char* readable
 */
class MemStream : public std::istream {
private:
    class MemBuf : public std::basic_streambuf<char> {
    public:
        MemBuf(const void *data, int dataLen) {
            setg((char*)data, (char*)data, (char*)data + dataLen);
        }
    };
    MemBuf _buffer;
public:
    MemStream(const void* data, int dataLen)
        :std::istream(&_buffer), _buffer(data, dataLen) {
            rdbuf(&_buffer);
        }
};

static std::vector<uint16_t>
convertMqttPayload(const MqttObjectBase& command, const void* data, int dataLen, bool expectedScalar) {
    std::vector<uint16_t> ret;

    switch(command.mPayloadType) {
        case MqttObjectCommand::PayloadType::STRING: {
            MemStream stream(data, dataLen);
            // istream will treat negative number as signed and then force it in unsigned
            // So use a larger int to detect invalid values
            int32_t temp;
            while (!stream.eof() && stream >> temp) {
                if (temp > UINT16_MAX || temp < 0) {
                    throw MqttPayloadConversionException(std::string("Conversion to uint16 failed, value " + std::to_string(temp) + " out of range"));
                }
                ret.push_back(static_cast<uint16_t>(temp));
            }
            if (stream.fail()) {
                throw MqttPayloadConversionException(std::string("Conversion to number failed, value " + std::string((char*)data, std::min(dataLen, 128))));
            }
        } break;
        default:
          throw MqttPayloadConversionException("Conversion failed, unknown payload type" + std::to_string(command.mPayloadType));
    }
    if (expectedScalar && ret.size() != 1) {
        throw MqttPayloadConversionException(std::string("Conversion to scalar failed, value " + std::string((char*)data, std::min(dataLen, 128))));
    }

    switch(command.mRegister.mRegisterType) {
        case RegisterType::BIT:
        case RegisterType::COIL:
            for (auto it = ret.begin(); it != ret.end(); ++it) {
                if (*it != 0 && *it != 1) {
                    throw MqttPayloadConversionException(std::string("Conversion failed, cannot convert " + std::to_string(*it) + " to single bit"));
                }
            }
        break;
    }
    return ret;
}

void
MqttClient::onMessage(const char* topic, const void* payload, int payloadLen, const modmqttd::MqttPublishProps& md) {
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
            if (command.isSameAs(typeid(MqttObjectRemoteCall))) {
                // RPC for read/write
                if (md.mResponseTopic.size() == 0) {
                    BOOST_LOG_SEV(log, Log::error) << "Empty response topic for remote call  " << topic << ", dropping message";
                } else {
                    if (payloadLen == 0) {
                        (*it)->sendReadCommand(static_cast<const MqttObjectRemoteCall&>(command), md);
                    } else {
                        std::vector<uint16_t> value = convertMqttPayload(command, payload, payloadLen, false);
                        (*it)->sendWriteCommand(command, value, md);
                    }
                }
            } else {
                // Read single value
                std::vector<uint16_t> value = convertMqttPayload(command, payload, payloadLen, true);
                (*it)->sendCommand(command, value[0]);
            }
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
        // Search in commands
        std::vector<MqttObjectCommand>::const_iterator cmd = std::find_if(
            obj->mCommands.begin(), obj->mCommands.end(),
            [&commandName](const MqttObjectCommand& item) -> bool { return item.mName == commandName; }
        );
        if (cmd != obj->mCommands.end()) {
            return *cmd;
        }
        // Else search in rpc
        std::vector<MqttObjectRemoteCall>::const_iterator rpc = std::find_if(
            obj->mRemoteCalls.begin(), obj->mRemoteCalls.end(),
            [&commandName](const MqttObjectRemoteCall& item) -> bool { return item.mName == commandName; }
        );
        if (rpc != obj->mRemoteCalls.end()) {
            return *rpc;
        }
    }
    throw ObjectCommandNotFoundException(topic);
}

void 
MqttClient::processRemoteCallResponse(const MqttObjectRegisterIdent& ident, const MqttPublishProps& responseProps, std::vector<uint16_t> data) {
    std::stringstream str;
    bool first = true;
    for (auto it = data.begin(); it != data.end(); ++it, first = false) {
        ((!first) ? (str << " ") : str) << *it ;
    }
    std::string msg = str.str();
    mMqttImpl->publish(responseProps.mResponseTopic.c_str(), msg.size(), msg.c_str(), responseProps);
}

void 
MqttClient::processRemoteCallResponseError(const MqttObjectRegisterIdent& ident, const MqttPublishProps& responseProps, std::string error) {
    // Override type
    MqttPublishProps props = responseProps;
    mMqttImpl->publish(responseProps.mResponseTopic.c_str(), error.size(), error.c_str(), props);
}

}
