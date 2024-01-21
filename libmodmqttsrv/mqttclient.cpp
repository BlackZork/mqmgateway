#include <cstring>
#include <map>

#include "mqttclient.hpp"
#include "exceptions.hpp"
#include "modmqtt.hpp"
#include "default_command_converter.hpp"

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
	BOOST_LOG_SEV(log, Log::info) << "Mqtt connected, sending subscriptions…";

    for(std::vector<MqttObject>::const_iterator obj = mObjects.begin(); obj != mObjects.end(); obj++)
        for(std::vector<MqttObjectCommand>::const_iterator it = obj->mCommands.begin(); it != obj->mCommands.end(); it++)
            subscribeToCommandTopic(obj->getTopic(), *it);

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
MqttClient::subscribeToCommandTopic(const std::string& objectTopic, const MqttObjectCommand& cmd) {
    std::string topic = objectTopic + "/" + cmd.mName;
    mMqttImpl->subscribe(topic.c_str());
}

void
MqttClient::processRegisterValues(const std::string& modbusNetworkName, const MsgRegisterValues& slaveData) {
    if (!isConnected()) {
        // we drop changes when there is no connection
        // retain flag is set so
        // broker will send last known value for us.
        return;
    }

    std::map<MqttObject*, AvailableFlag> modified;

    MqttObjectRegisterIdent ident(modbusNetworkName, slaveData);
    for(auto& obj: mObjects)
    {
        int regIndex = 0;
        AvailableFlag oldAvail = obj.getAvailableFlag();
        int lastRegister = slaveData.mRegisterNumber + slaveData.mCount;
        for(int regNumber = slaveData.mRegisterNumber; regNumber < lastRegister; regNumber++) {
            ident.mRegisterNumber = regNumber;
            uint16_t value = slaveData.mRegisters.getValue(regIndex++);
            if (!obj.updateRegisterValue(ident, value))
                continue;

            modified[&obj] = oldAvail;
        }
    }

    for(auto& obj_it: modified) {
        MqttObject& obj(*(obj_it.first));
        if (obj.mState.hasValues()) {
            publishState(obj);
            AvailableFlag newAvail = obj.getAvailableFlag();
            if (obj_it.second != newAvail)
                publishAvailabilityChange(obj);
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
MqttClient::processRegistersOperationFailed(const std::string& modbusNetworkName, const MsgRegisterMessageBase& slaveData) {
    std::map<MqttObject*, AvailableFlag> modified;

    MqttObjectRegisterIdent ident(modbusNetworkName, slaveData);
    for(auto& obj: mObjects)
    {
        int regIndex = 0;
        AvailableFlag oldAvail = obj.getAvailableFlag();
        int lastRegister = slaveData.mRegisterNumber + slaveData.mCount;
        for(int regNumber = slaveData.mRegisterNumber; regNumber < lastRegister; regNumber++) {
            ident.mRegisterNumber = regNumber;
            if (!obj.updateRegisterReadFailed(ident))
                continue;

            modified[&obj] = oldAvail;
        }
    }

    for(auto& obj_it: modified) {
        MqttObject& obj(*(obj_it.first));
        publishState(obj);
        AvailableFlag newAvail = obj.getAvailableFlag();
        if (obj_it.second != newAvail)
            publishAvailabilityChange(obj);
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
        const std::string network = command.mRegister.mNetworkName;

        //TODO is is thread safe to iterate on modbus clients from mosquitto callback?
        std::vector<std::shared_ptr<ModbusClient>>::const_iterator it = std::find_if(
            mModbusClients.begin(), mModbusClients.end(),
            [&network](const std::shared_ptr<ModbusClient>& client) -> bool { return client->mName == network; }
        );
        if (it == mModbusClients.end()) {
            BOOST_LOG_SEV(log, Log::error) << "Modbus network " << network << " not found for command  " << topic << ", dropping message";
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
        BOOST_LOG_SEV(log, Log::error) << "Converter error for " << topic << ":" << ex.what();
    } catch (const MqttPayloadConversionException& ex) {
        BOOST_LOG_SEV(log, Log::error) << "Value error for " << topic << ":" << ex.what();
    } catch (const ObjectCommandNotFoundException&) {
        BOOST_LOG_SEV(log, Log::error) << "No command for topic " << topic << ", dropping message";
    }
}

const MqttObjectCommand&
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
