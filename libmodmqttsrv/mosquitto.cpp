#include <cstring>

#include "mosquitto.hpp"
#include "exceptions.hpp"
#include "mqttclient.hpp"

namespace modmqttd {

void
Mosquitto::throwOnCriticalError(int code) {
    // copied from mosquittopp loop_forever()
    switch(code) {
        case MOSQ_ERR_NOMEM:
        case MOSQ_ERR_PROTOCOL:
        case MOSQ_ERR_INVAL:
        case MOSQ_ERR_NOT_FOUND:
        case MOSQ_ERR_TLS:
        case MOSQ_ERR_PAYLOAD_SIZE:
        case MOSQ_ERR_NOT_SUPPORTED:
        case MOSQ_ERR_AUTH:
        case MOSQ_ERR_ACL_DENIED:
        case MOSQ_ERR_UNKNOWN:
        case MOSQ_ERR_EAI:
        case MOSQ_ERR_PROXY:
            throw MosquittoException(returnCodeToStr(code));
    }
}

void
Mosquitto::connect(const MqttBrokerConfig& config) {
    BOOST_LOG_SEV(log, Log::info) << "Connecting to " << config.mHost << ":" << config.mPort;
    int rc = connect_async(config.mHost.c_str(),
            config.mPort,
            config.mKeepalive);
    throwOnCriticalError(rc);
    if(errno == EPROTO) {
        throw MosquittoException(std::strerror(errno));
    }

    if (rc != MOSQ_ERR_SUCCESS) {
        BOOST_LOG_SEV(log, Log::error) << "Error connecting to mqtt broker: " << returnCodeToStr(rc);
    } else {
        BOOST_LOG_SEV(log, Log::info) << "Connection estabilished";
        reconnect_delay_set(3,60, true);
        loop_start();
    }
}

void Mosquitto::init(MqttClient* owner, const char* clientId) {
    mOwner = owner;
    reinitialise(clientId, true);
}

void
Mosquitto::subscribe(const char* topic) {
    int msgId;
    mosquittopp::subscribe(&msgId, topic);
}

void
Mosquitto::publish(const char* topic, int len, const void* data) {
    int msgId;
    mosquittopp::publish(&msgId, topic, len, data, 0, true);
}


void
Mosquitto::on_disconnect(int rc) {
    BOOST_LOG_SEV(log, Log::info) << "Disconnected from mqtt broker, code:" << returnCodeToStr(rc);
    mOwner->onDisconnect();
}

void
Mosquitto::on_connect(int rc) {
    mOwner->onConnect();
}

void
Mosquitto::on_log(int level, const char* message) {
    switch(level) {
        case MOSQ_LOG_INFO:
            BOOST_LOG_SEV(log, Log::info) << message;
        break;
        case MOSQ_LOG_NOTICE:
            BOOST_LOG_SEV(log, Log::info) << message;
        break;
        case MOSQ_LOG_WARNING:
            BOOST_LOG_SEV(log, Log::warn) << message;
        break;
        case  MOSQ_LOG_ERR:
            BOOST_LOG_SEV(log, Log::error) << message;
        break;
        case MOSQ_LOG_DEBUG:
            BOOST_LOG_SEV(log, Log::debug) << message;
        break;
    }
}

void
Mosquitto::on_message(const struct mosquitto_message *message) {
    mOwner->onMessage(message->topic, message->payload, message->payloadlen);
}

const char*
Mosquitto::returnCodeToStr(int mosq_errno) {
    // copied from mosquitto.c
	switch(mosq_errno){
		case MOSQ_ERR_CONN_PENDING:
			return "Connection pending.";
		case MOSQ_ERR_SUCCESS:
			return "No error.";
		case MOSQ_ERR_NOMEM:
			return "Out of memory.";
		case MOSQ_ERR_PROTOCOL:
			return "A network protocol error occurred when communicating with the broker.";
		case MOSQ_ERR_INVAL:
			return "Invalid function arguments provided.";
		case MOSQ_ERR_NO_CONN:
			return "The client is not currently connected.";
		case MOSQ_ERR_CONN_REFUSED:
			return "The connection was refused.";
		case MOSQ_ERR_NOT_FOUND:
			return "Message not found (internal error).";
		case MOSQ_ERR_CONN_LOST:
			return "The connection was lost.";
		case MOSQ_ERR_TLS:
			return "A TLS error occurred.";
		case MOSQ_ERR_PAYLOAD_SIZE:
			return "Payload too large.";
		case MOSQ_ERR_NOT_SUPPORTED:
			return "This feature is not supported.";
		case MOSQ_ERR_AUTH:
			return "Authorisation failed.";
		case MOSQ_ERR_ACL_DENIED:
			return "Access denied by ACL.";
		case MOSQ_ERR_UNKNOWN:
			return "Unknown error.";
		case MOSQ_ERR_EAI:
			return "Lookup error.";
		case MOSQ_ERR_PROXY:
			return "Proxy error.";
        case MOSQ_ERR_ERRNO:
            return std::strerror(errno);
		default:
			return "Unknown error.";
	}
};

}
