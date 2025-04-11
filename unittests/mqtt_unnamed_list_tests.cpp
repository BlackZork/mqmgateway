#include "catch2/catch_all.hpp"
#include "mockedserver.hpp"
#include "jsonutils.hpp"
#include "defaults.hpp"
#include "yaml_utils.hpp"

TEST_CASE ("Unnamed state list without register value") {

TestConfig config(R"(
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
)");

SECTION ("should not be available") {
    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 1);
    server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::INPUT, 7);
    server.setModbusRegisterReadError("tcptest", 1, 3, modmqttd::RegisterType::INPUT);

    server.start();

    //wait for 4sec for three read attempts
    server.waitForPublish("test_state/availability", defaultWaitTime(std::chrono::milliseconds(4000)));
    REQUIRE(server.mqttValue("test_state/availability") == "0");
    server.stop();
}


SECTION ("should output json array") {
    MockedModMqttServerThread server(config.toString());
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

SECTION ("should output single element json array") {
    config.mYAML["mqtt"]["objects"][0]["state"].remove(1);
    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 6);
    server.start();

    //to make sure that all registers have initial value
    server.waitForPublish("test_state/availability");
    REQUIRE(server.mqttValue("test_state/availability") == "1");

    server.waitForPublish("test_state/state");

    REQUIRE_JSON(server.mqttValue("test_state/state"), "[6]");
    server.stop();
}


}

TEST_CASE ("Unnamed state list declared as registers") {

TestConfig config(R"(
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
        registers:
          - register: tcptest.1.2
          - register: tcptest.1.3
)");

SECTION ("should output json array") {
    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 1);
    server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, 7);
    server.start();

    //to make sure that all registers have initial value
    server.waitForPublish("test_state/availability");
    REQUIRE(server.mqttValue("test_state/availability") == "1");

    server.waitForPublish("test_state/state");

    REQUIRE(server.mModbusFactory->getMockedModbusContext("tcptest").getReadCount(1) == 2);
    REQUIRE_JSON(server.mqttValue("test_state/state"), "[1,7]");
    server.stop();
}

SECTION ("should output a single element json array") {
    config.mYAML["mqtt"]["objects"][0]["state"]["registers"].remove(1);
    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 5);
    server.start();

    //to make sure that all registers have initial value
    server.waitForPublish("test_state/availability");
    REQUIRE(server.mqttValue("test_state/availability") == "1");

    server.waitForPublish("test_state/state");

    REQUIRE_JSON(server.mqttValue("test_state/state"), "[5]");
    server.stop();
}


}


TEST_CASE ("Unnamed state list declared as starting register and count") {

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

SECTION ("should output json array") {
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

SECTION ("should write from json array") {
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

}


TEST_CASE ("Unnamed state list declared as register list with count") {

TestConfig config(R"(
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
          count: 2
        - register: tcptest.1.4
          count: 2
)");

SECTION ("should output nested json array") {
    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 1);
    server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, 0);
    server.setModbusRegisterValue("tcptest", 1, 4, modmqttd::RegisterType::HOLDING, 7);
    server.setModbusRegisterValue("tcptest", 1, 5, modmqttd::RegisterType::HOLDING, 0);
    server.start();

    //to make sure that all registers have initial value
    server.waitForPublish("test_state/availability");
    REQUIRE(server.mqttValue("test_state/availability") == "1");

    server.waitForPublish("test_state/state");

    REQUIRE(server.mModbusFactory->getMockedModbusContext("tcptest").getReadCount(1) == 2);
    REQUIRE_JSON(server.mqttValue("test_state/state"), "[[1,0],[7,0]]");
    server.stop();
}


SECTION ("should output nested single element array #65") {
    config.mYAML["mqtt"]["objects"][0]["state"].remove(1);
    MockedModMqttServerThread server(config.toString());

    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 1);
    server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, 0);
    server.start();

    //to make sure that all registers have initial value
    server.waitForPublish("test_state/availability");
    REQUIRE(server.mqttValue("test_state/availability") == "1");

    server.waitForPublish("test_state/state");

    REQUIRE_JSON(server.mqttValue("test_state/state"), "[[1,0]]");
    server.stop();
}


}


TEST_CASE ("issue_87_bug") {

TestConfig config(R"(
modmqttd:
  converter_search_path:
    - build/stdconv
  converter_plugins:
    - stdconv.so
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
          converter: std.multiply(10)
)");

SECTION ("should json array with a single element") {
    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 8);
    server.start();

    server.waitForPublish("test_state/state");

    REQUIRE_JSON(server.mqttValue("test_state/state"), "[8]");
    server.stop();
}

}


