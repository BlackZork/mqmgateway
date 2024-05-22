#include <cstring>
#include <cassert>
#include <map>

#include "common.hpp"
#include "mqttclient.hpp"
#include "exceptions.hpp"
#include "modmqtt.hpp"
#include "default_command_converter.hpp"

namespace modmqttd {

boost::log::sources::severity_logger<Log::severity> MqttClient::log;

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
	BOOST_LOG_SEV(log, Log::info) << "Mqtt connected, sending subscriptions…";

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

	BOOST_LOG_SEV(log, Log::info) << "Mqtt ready to process messages";
}

void
MqttClient::processRegisterValues(const std::string& pModbusNetworkName, const MsgRegisterValues& pSlaveData) {
    if (!isConnected()) {
        // we drop changes when there is no connection
        // retain flag is set so
        // broker will send last known value for us.
        return;
    }

    MqttObjectRegisterIdent ident(pModbusNetworkName, pSlaveData);
    MqttObjMap::iterator it = mObjects.find(ident);

    //we do not poll if there is nothing to publish
    assert(it != mObjects.end());

    for (std::shared_ptr<MqttObject>& obj: it->second) {
        AvailableFlag oldAvail = obj->mAvailability.getAvailableFlag();
        bool stateChanged = obj->updateRegisterValues(pModbusNetworkName, pSlaveData);
        AvailableFlag newAvail = obj->mAvailability.getAvailableFlag();

        if (oldAvail != newAvail) {
            publishState(*obj);
            publishAvailabilityChange(*obj);
        } else {
            if (stateChanged)
                publishState(*obj);
        }
    }

/*
    std::map<MqttObject*, AvailableFlag> modified;

    MqttObjectRegisterIdent ident(modbusNetworkName, slaveData);
    for(auto& obj: mObjects)
    {
        int regIndex = 0;
        AvailableFlag oldAvail = obj.getAvailableFlag();
        int lastRegister = slaveData.mRegister + slaveData.mCount;
        for(int regNumber = slaveData.mRegister; regNumber < lastRegister; regNumber++) {
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
    */
}

void
MqttClient::publishState(const MqttObject& obj) {
    int msgId;
    std::string messageData(obj.mState.createMessage());
    BOOST_LOG_SEV(log, Log::debug) << "Publish on topic " << obj.getStateTopic() << ": " << messageData;
    mMqttImpl->publish(obj.getStateTopic().c_str(), messageData.length(), messageData.c_str());
}

void
MqttClient::processRegistersOperationFailed(const std::string& pModbusNetworkName, const MsgRegisterMessageBase& pSlaveData) {
/*    std::map<MqttObject*, AvailableFlag> modified;

    MqttObjectRegisterIdent ident(modbusNetworkName, slaveData);
    for(auto& obj: mObjects)
    {
        int regIndex = 0;
        AvailableFlag oldAvail = obj.mAvailability.getAvailableFlag();
        int lastRegister = slaveData.mRegister + slaveData.mCount;
        for(int regNumber = slaveData.mRegister; regNumber < lastRegister; regNumber++) {
            ident.mRegisterNumber = regNumber;
            if (!obj.updateRegisterReadFailed(ident))
                continue;

            modified[&obj] = oldAvail;
        }
    }

    for(auto& obj_it: modified) {
        MqttObject& obj(*(obj_it.first));
        AvailableFlag newAvail = obj.mAvailability.getAvailableFlag();
        if (obj_it.second != newAvail)
            publishAvailabilityChange(obj);
    }
*/

    MqttObjectRegisterIdent ident(pModbusNetworkName, pSlaveData);
    MqttObjMap::iterator it = mObjects.find(ident);

    //we do not poll if there is nothing to publish
    assert(it != mObjects.end());

    for (std::shared_ptr<MqttObject>& obj: it->second) {
        AvailableFlag oldAvail = obj->mAvailability.getAvailableFlag();
        bool stateChanged = obj->updateRegistersReadFailed(pModbusNetworkName, pSlaveData);
        AvailableFlag newAvail = obj->mAvailability.getAvailableFlag();

        if (oldAvail != newAvail) {
            publishState(*obj);
            publishAvailabilityChange(*obj);
        } else {
            if (stateChanged)
                publishState(*obj);
        }
    }
}

void
MqttClient::processModbusNetworkState(const std::string& pNetworkName, bool pIsUp) {
    for(MqttObjMap::iterator it = mObjects.begin(); it != mObjects.end(); it++)
    {
        if (it->first.mNetworkName != pNetworkName)
            continue;

        for (std::vector<std::shared_ptr<MqttObject>>::iterator oit = it->second.begin(); oit != it->second.end(); oit++) {
            AvailableFlag oldAvail = (*oit)->getAvailableFlag();
            (*oit)->setModbusNetworkState(pNetworkName, pIsUp);
            if (oldAvail != (*oit)->getAvailableFlag())
                publishAvailabilityChange(**oit);
        }
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
    for(MqttObjMap::iterator it = mObjects.begin(); it != mObjects.end(); it++)
    {
        for (std::vector<std::shared_ptr<MqttObject>>::iterator oit = it->second.begin(); oit != it->second.end(); oit++) {
            if ((*oit)->getAvailableFlag() == AvailableFlag::True)
                publishState(**oit);
            publishAvailabilityChange(**oit);
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
