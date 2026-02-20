#include <catch2/catch_all.hpp>
#include "mockedserver.hpp"
#include "yaml_utils.hpp"


TEST_CASE("Write single register") {

TestConfig config(R"(
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
)");

SECTION ("holding type") {
    MockedModMqttServerThread server(config.toString());
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

SECTION ("coil type") {
    MockedModMqttServerThread server(config.toString());
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

SECTION ("with invalid coil value should not crash server") {
    MockedModMqttServerThread server(config.toString());
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

} //CASE


TEST_CASE ("Command topic") {

TestConfig config(R"(
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
)");

SECTION("with subpath should map to valid object") {
    MockedModMqttServerThread server(config.toString());
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

SECTION("with path in command name should map to valid object") {
    config.mYAML["mqtt"]["objects"][0]["commands"][0]["name"] = "command/set";

    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 0);
    server.start();

    server.waitForPublish("some/subpath/test_switch/availability");
    REQUIRE(server.mqttValue("some/subpath/test_switch/availability") == "1");
    server.waitForPublish("some/subpath/test_switch/state");

    server.publish("some/subpath/test_switch/command/set", "32");

    server.waitForPublish("some/subpath/test_switch/state");
    REQUIRE(server.mqttValue("some/subpath/test_switch/state") == "32");

    server.stop();
}


}


TEST_CASE ("Write multiple registers with default converter") {
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
)");


    MockedModMqttServerThread server(config.toString());
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
