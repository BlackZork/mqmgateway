#include "catch2/catch_all.hpp"
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
  broker:
    host: localhost
  objects:
    - topic: test_switch
      commands:
        - name: set
          register: tcptest.1.2
          register_type: holding
      state:
        register: tcptest.1.2
        register_type: holding
)";

TEST_CASE ("Holding register valid write") {
    MockedModMqttServerThread server(config);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 0);
    server.start();

    server.waitForPublish("test_switch/availability");
    REQUIRE(server.mqttValue("test_switch/availability") == "1");
    server.waitForPublish("test_switch/state");

    server.publish("test_switch/set", "32");

    server.waitForPublish("test_switch/state");
    REQUIRE(server.mqttValue("test_switch/state") == "32");

    server.stop();
}

TEST_CASE ("Coil register valid write") {
    MockedModMqttServerThread server(config);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::COIL, 0);
    server.start();

    server.waitForPublish("test_switch/availability");
    REQUIRE(server.mqttValue("test_switch/availability") == "1");
    server.waitForPublish("test_switch/state");

    server.publish("test_switch/set", "1");

    server.waitForPublish("test_switch/state");
    REQUIRE(server.mqttValue("test_switch/state") == "1");

    server.stop();
}

TEST_CASE ("Mqtt invalid value should not crash server") {
    MockedModMqttServerThread server(config);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::COIL, 0);
    server.start();


    server.waitForPublish("test_switch/availability");
    REQUIRE(server.mqttValue("test_switch/availability") == "1");
    server.waitForPublish("test_switch/state");

    server.publish("test_switch/set", "hello, world!");
    server.publish("test_switch/set", "1");
    server.waitForPublish("test_switch/state");
    REQUIRE(server.mqttValue("test_switch/state") == "1");

    server.stop();
}


static const std::string config_subpath = R"(
modbus:
  networks:
    - name: tcptest
      address: localhost
      port: 501
mqtt:
  client_id: mqtt_test
  broker:
    host: localhost
  objects:
    - topic: some/subpath/test_switch
      commands:
        - name: set
          register: tcptest.1.2
          register_type: holding
      state:
        register: tcptest.1.2
        register_type: holding
)";
TEST_CASE ("Valid write to subpath topic") {
    MockedModMqttServerThread server(config_subpath);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 0);
    server.start();

    server.waitForPublish("some/subpath/test_switch/availability");
    REQUIRE(server.mqttValue("some/subpath/test_switch/availability") == "1");
    server.waitForPublish("some/subpath/test_switch/state");

    server.publish("some/subpath/test_switch/set", "32");

    server.waitForPublish("some/subpath/test_switch/state");
    REQUIRE(server.mqttValue("some/subpath/test_switch/state") == "32");

    server.stop();
}


static const std::string config_multiple = R"(
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
  broker:
    host: localhost
  objects:
    - topic: test_switch
      commands:
        - name: set
          register: tcptest.1.2
          register_type: holding
          count: 2
      state:
        register: tcptest.1.2
        register_type: holding
        count: 2
        converter: std.int32()
)";
TEST_CASE ("Write multiple registers with default converter") {
    MockedModMqttServerThread server(config_multiple);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 0);
    server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, 0);
    server.start();

    server.waitForPublish("test_switch/availability");
    REQUIRE(server.mqttValue("test_switch/availability") == "1");
    server.waitForPublish("test_switch/state");
    REQUIRE(server.mqttValue("test_switch/state") == "0");

    server.publish("test_switch/set", "[2,1]");

    server.waitForPublish("test_switch/state");
    REQUIRE(server.mqttValue("test_switch/state") == "131073");
    REQUIRE(server.mModbusFactory->getMockedModbusContext("tcptest").getWriteCount(1) == 1);

    server.stop();
}
