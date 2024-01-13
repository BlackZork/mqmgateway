#include "catch2/catch_all.hpp"
#include "mockedserver.hpp"
#include "jsonutils.hpp"
#include "defaults.hpp"

static const std::string config1 = R"(
modbus:
  networks:
    - name: tcptest
      address: localhost
      port: 501
mqtt:
  client_id: mqtt_test
  refresh: 1s
  broker:
    host: localhost
  objects:
    - topic: test_state
      state:
        - register: tcptest.1.2
          register_type: input
        - register: tcptest.1.3
          register_type: input
)";

TEST_CASE ("Unnamed state list without register value should not be available") {
    MockedModMqttServerThread server(config1);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 1);
    server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::INPUT, 7);
    server.setModbusRegisterReadError("tcptest", 1, 3, modmqttd::RegisterType::INPUT);

    server.start();

    //wait for 4sec for three read attempts
    server.waitForPublish("test_state/availability", defaultWaitTime(std::chrono::milliseconds(4000)));
    REQUIRE(server.mqttValue("test_state/availability") == "0");
    server.stop();
}


TEST_CASE ("Unnamed state list should output json array") {
    MockedModMqttServerThread server(config1);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 1);
    server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::INPUT, 7);
    server.start();

    //to make sure that all registers have initial value
    server.waitForPublish("test_state/availability");
    REQUIRE(server.mqttValue("test_state/availability") == "1");

    server.waitForPublish("test_state/state");

    REQUIRE(server.mModbusFactory->getMockedModbusContext("tcptest").getReadCount(1) == 2);
    REQUIRE_JSON(server.mqttValue("test_state/state"), "[1,7]");
    server.stop();
}

static const std::string config2 = R"(
modbus:
  networks:
    - name: tcptest
      address: localhost
      port: 501
mqtt:
  client_id: mqtt_test
  refresh: 1s
  broker:
    host: localhost
  objects:
    - topic: test_state
      state:
        register: tcptest.1.2
        count: 2
      commands:
        - name: set
          register: tcptest.1.2
          count: 2
)";

TEST_CASE ("Unnamed state list declared as starting register and count should output json array") {
    MockedModMqttServerThread server(config2);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 2);
    server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, 4);
    server.start();

    //to make sure that all registers have initial value
    server.waitForPublish("test_state/availability");
    REQUIRE(server.mqttValue("test_state/availability") == "1");

    server.waitForPublish("test_state/state");

    REQUIRE(server.mModbusFactory->getMockedModbusContext("tcptest").getReadCount(1) == 1);
    REQUIRE_JSON(server.mqttValue("test_state/state"), "[2,4]");
    server.stop();
}

TEST_CASE ("Unnamed state list declared as starting register and count should write from json array") {
    MockedModMqttServerThread server(config2);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 0);
    server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, 0);
    server.start();

    //to make sure that all registers have initial value
    server.waitForPublish("test_state/availability");
    REQUIRE(server.mqttValue("test_state/availability") == "1");

    server.waitForPublish("test_state/state");
    REQUIRE_JSON(server.mqttValue("test_state/state"), "[0,0]");

    server.publish("test_state/set", "[3,4]");
    server.waitForPublish("test_state/state");
    REQUIRE_JSON(server.mqttValue("test_state/state"), "[3,4]");

    server.stop();
}

