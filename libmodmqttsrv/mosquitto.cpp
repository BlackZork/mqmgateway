#include <cstring>

#include "mosquitto.hpp"
#include "exceptions.hpp"
#include "mqttclient.hpp"

namespace modmqttd {

static void on_connect_wrapper(struct mosquitto *mosq, void *userdata, int rc)
{
	class Mosquitto *m = (class Mosquitto *)userdata;
	m->on_connect(rc);
}


static void on_connect_with_flags_wrapper(struct mosquitto *mosq, void *userdata, int rc, int flags)
{
	class Mosquitto *m = (class Mosquitto *)userdata;
//	m->on_connect_with_flags(rc, flags);
}


static void on_disconnect_wrapper(struct mosquitto *mosq, void *userdata, int rc)
{
	class Mosquitto *m = (class Mosquitto *)userdata;
	m->on_disconnect(rc);
}

/*
static void on_publish_wrapper(struct mosquitto *mosq, void *userdata, int mid)
{
	class Mosquitto *m = (class Mosquitto *)userdata;
	m->on_publish(mid);
}
*/

static void on_message_wrapper(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message, const mosquitto_property *props)
{
	class Mosquitto *m = (class Mosquitto *)userdata;
	m->on_message(message, props);
}

/*
static void on_subscribe_wrapper(struct mosquitto *mosq, void *userdata, int mid, int qos_count, const int *granted_qos)
{
	class Mosquitto *m = (class Mosquitto *)userdata;
	m->on_subscribe(mid, qos_count, granted_qos);
}

static void on_unsubscribe_wrapper(struct mosquitto *mosq, void *userdata, int mid)
{
	class Mosquitto *m = (class Mosquitto *)userdata;
	m->on_unsubscribe(mid);
}
*/

static void on_log_wrapper(struct mosquitto *mosq, void *userdata, int level, const char *str)
{
	class Mosquitto *m = (class Mosquitto *)userdata;
	m->on_log(level, str);
}

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
Mosquitto::libInit() {
    mosquitto_lib_init();
}

void
Mosquitto::libCleanup() {
    mosquitto_lib_cleanup();
}

Mosquitto::Mosquitto() {
	mMosq = mosquitto_new(NULL, true, this);
}

void
Mosquitto::connect(const MqttBrokerConfig& config) {
    BOOST_LOG_SEV(log, Log::info) << "Connecting to " << config.mHost << ":" << config.mPort;
    int rc = mosquitto_connect_async(mMosq, config.mHost.c_str(),
            config.mPort,
            config.mKeepalive);
    throwOnCriticalError(rc);
    if(errno == EPROTO) {
        throw MosquittoException(std::strerror(errno));
    }

    if (rc != MOSQ_ERR_SUCCESS) {
        BOOST_LOG_SEV(log, Log::error) << "Error connecting to mqtt broker: " << returnCodeToStr(rc);
    } else {
        if (config.mUseRpc) {
            BOOST_LOG_SEV(log, Log::info) << "Using protocol version 5";
            mosquitto_int_option(mMosq, MOSQ_OPT_PROTOCOL_VERSION, MQTT_PROTOCOL_V5);
        }
        mosquitto_reconnect_delay_set(mMosq, 3, 60, true);
        mosquitto_connect_callback_set(mMosq, on_connect_wrapper);
        mosquitto_connect_with_flags_callback_set(mMosq, on_connect_with_flags_wrapper);
        mosquitto_disconnect_callback_set(mMosq, on_disconnect_wrapper);
        //mosquitto_publish_callback_set(mMosq, on_publish_wrapper);
        mosquitto_message_v5_callback_set(mMosq, on_message_wrapper);
        //mosquitto_subscribe_callback_set(mMosq, on_subscribe_wrapper);
        //mosquitto_unsubscribe_callback_set(mMosq, on_unsubscribe_wrapper);
        mosquitto_log_callback_set(mMosq, on_log_wrapper);


        BOOST_LOG_SEV(log, Log::debug) << "Waiting for connection event";
        int rc = mosquitto_loop_start(mMosq);
        if (rc != MOSQ_ERR_SUCCESS) {
            BOOST_LOG_SEV(log, Log::error) << "Error processing network traffic: " << returnCodeToStr(rc);
        }
    }
}

void
Mosquitto::reconnect() {
    mosquitto_reconnect(mMosq);
}

void
Mosquitto::disconnect() {
    mosquitto_disconnect(mMosq);
}

void
Mosquitto::stop() {
    mosquitto_loop_stop(mMosq, false);
}

void Mosquitto::init(MqttClient* owner, const char* clientId) {
    mOwner = owner;
    //TODO check return code?
	mosquitto_reinitialise(mMosq, clientId, true, this);
}

void
Mosquitto::subscribe(const char* topic) {
    int msgId;
    mosquitto_subscribe(mMosq, &msgId, topic, 0);
}

void
Mosquitto::publish(const char* topic, int len, const void* data) {
    int msgId;
    mosquitto_publish(mMosq, &msgId, topic, len, data, 0, true);
}

void
Mosquitto::publish(const char* topic, int len, const void* data, const MqttPublishProps& md) {
    int msgId;
    mosquitto_property *proplist = nullptr;
    if (md.mResponseTopic.size() > 0) {
        mosquitto_property_add_string(&proplist, MQTT_PROP_RESPONSE_TOPIC, md.mResponseTopic.c_str());
    }
    if (md.mCorrelationData.size() > 0) {
        mosquitto_property_add_binary(&proplist, MQTT_PROP_CORRELATION_DATA, &md.mCorrelationData[0], md.mCorrelationData.size());
    }
    mosquitto_publish_v5(mMosq, &msgId, topic, len, data, 0, true, proplist);
    mosquitto_property_free_all(&proplist);
}


void
Mosquitto::on_disconnect(int rc) {
    BOOST_LOG_SEV(log, Log::info) << "Disconnected from mqtt broker, code:" << returnCodeToStr(rc);
    mOwner->onDisconnect();
}

void
Mosquitto::on_connect(int rc) {
    BOOST_LOG_SEV(log, Log::info) << "Connection estabilished";
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
Mosquitto::on_message(const struct mosquitto_message *message, const mosquitto_property *props) {
    // Only there for v5 protocol
    MqttPublishProps pubProps;
    if (props != nullptr) {
        char *responseTopic = nullptr;
        if (mosquitto_property_read_string(props, MQTT_PROP_RESPONSE_TOPIC, &responseTopic, false)) {
            pubProps.mResponseTopic = responseTopic;
        }
        uint8_t* correlationBuffer;
        uint16_t correlationLen;
        if (mosquitto_property_read_binary(props, MQTT_PROP_CORRELATION_DATA, reinterpret_cast<void**>(&correlationBuffer), &correlationLen, false)) {
            pubProps.mCorrelationData = std::vector<uint8_t>(correlationBuffer, correlationBuffer + correlationLen);
        }
    }
    mOwner->onMessage(message->topic, message->payload, message->payloadlen, pubProps);
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

Mosquitto::~Mosquitto() {
	mosquitto_destroy(mMosq);
}

}
