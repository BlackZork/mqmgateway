#include <thread>
#include "catch2/catch.hpp"
#include "mockedserver.hpp"
#include "defaults.hpp"

static const std::string config = R"(
modbus:
  networks:
    - name: tcptest
      address: localhost
      port: 501
mqtt:
  client_id: mqtt_test
  refresh: 50ms
  broker:
    host: localhost
  objects:
    - topic: test_call
      remote_calls:
        - name: range
          register: tcptest.1.2
          register_type: holding
          size: 2
      availability: false
)";

// Better catch2 stack print in case of failure
#define REQUIRE_PUBLISH(server, topic, timeout) REQUIRE(server.checkForPublish(topic, timeout) == true)

TEST_CASE ("Check no availability published for a rpc object") {
    MockedModMqttServerThread server(config);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 23);
    server.start();
    REQUIRE(server.checkForPublish("test_call/availability", std::chrono::milliseconds(REGWAIT_MSEC)) == false);
    server.stop();
}

TEST_CASE ("Mqtt binary range write should work if configured") {
    MockedModMqttServerThread server(config);
    // Configure range 2-3, set a read/write error guards around
    server.setModbusRegisterError("tcptest", 1, 1, modmqttd::RegisterType::HOLDING);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 42);
    server.setModbusRegisterError("tcptest", 1, 4, modmqttd::RegisterType::HOLDING);
    server.start();

    std::this_thread::sleep_for(std::chrono::duration_cast<std::chrono::milliseconds>(REGWAIT_MSEC));

    // In little endian format
    modmqttd::MqttPublishProps props;
    props.mCorrelationData = { 1, 2, 3, 4 };
    props.mResponseTopic = "test_call/ack";
    server.publish("test_call/range", "43 44", props);

    REQUIRE_PUBLISH(server, "test_call/ack", REGWAIT_MSEC);
    REQUIRE(server.mqttValueProps("test_call/ack").mCorrelationData == std::vector<uint8_t>({ 1, 2, 3, 4 }));

    // But the writing impacted two registers
    REQUIRE(server.getModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING) == 43);
    REQUIRE(server.getModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING) == 44);

    // Do it again to test range limit, rejected because out of bounds
    server.publish("test_call/range", "53 54 55", props);
    std::this_thread::sleep_for(std::chrono::duration_cast<std::chrono::milliseconds>(REGWAIT_MSEC));
    REQUIRE(server.getModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING) == 43);
    REQUIRE(server.getModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING) == 44);

    // Do it again to test negative/invalid values, rejected
    server.publish("test_call/range", "53 -54", props);
    std::this_thread::sleep_for(std::chrono::duration_cast<std::chrono::milliseconds>(REGWAIT_MSEC));
    REQUIRE(server.getModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING) == 43);
    REQUIRE(server.getModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING) == 44);

    server.stop();
}

TEST_CASE ("Mqtt range read via Remote Call should work if configured") {
    MockedModMqttServerThread server(config);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 42);
    server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, 43);
    server.start();

    std::this_thread::sleep_for(std::chrono::duration_cast<std::chrono::milliseconds>(REGWAIT_MSEC));

    // In little endian format
    modmqttd::MqttPublishProps props;
    props.mCorrelationData = { 1, 2, 3, 4 };
    props.mResponseTopic = "test_call/read_back";
    server.publish("test_call/range", { }, props);

    REQUIRE_PUBLISH(server, "test_call/read_back", REGWAIT_MSEC);
    REQUIRE(server.mqttValue("test_call/read_back") == "42 43");
    REQUIRE(server.mqttValueProps("test_call/read_back").mCorrelationData == std::vector<uint8_t>({ 1, 2, 3, 4 }));

    // Check that availability and polling are not happening. Wait at least the global configured
    // refresh time
    std::this_thread::sleep_for(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::milliseconds(100)));
    REQUIRE(server.getModbusRegisterReadCount("tcptest", 1, 2, modmqttd::RegisterType::HOLDING) == 1);
    REQUIRE(server.getModbusRegisterReadCount("tcptest", 1, 3, modmqttd::RegisterType::HOLDING) == 1);

    // Test potential issues around int16 negative values
    // New correlation data
    server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, 65000);
    props.mCorrelationData = { 1, 2, 3, 5 };
    server.publish("test_call/range", { }, props);
    REQUIRE_PUBLISH(server, "test_call/read_back", REGWAIT_MSEC);
    REQUIRE(server.mqttValue("test_call/read_back") == "42 65000");
    REQUIRE(server.mqttValueProps("test_call/read_back").mCorrelationData == std::vector<uint8_t>({ 1, 2, 3, 5 }));

    // Test error due to disconnections
    server.disconnectModbusSlave("tcptest", 1);

    props.mCorrelationData = { 11, 12 };
    props.mResponseTopic = "test_call/read_back";
    server.publish("test_call/range", { }, props);

    REQUIRE_PUBLISH(server, "test_call/read_back", REGWAIT_MSEC);
    REQUIRE(server.mqttValue("test_call/read_back") == "libmodbus: read fn 2 failed: Input/output error");
    REQUIRE(server.mqttValueProps("test_call/read_back").mCorrelationData == std::vector<uint8_t>({ 11, 12 }));

    server.stop();
}

