#include "catch2/catch_all.hpp"
#include "mockedserver.hpp"
#include "defaults.hpp"

static const std::string config_converter = R"(
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
          converter: std.divide(2,0)
      state:
        register: tcptest.1.2
        register_type: holding
)";

TEST_CASE ("Write converted value") {
    MockedModMqttServerThread server(config_converter);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 0);
    server.start();

    server.waitForPublish("test_switch/availability");
    REQUIRE(server.mqttValue("test_switch/availability") == "1");
    server.waitForPublish("test_switch/state");

    server.publish("test_switch/set", "32");

    server.waitForPublish("test_switch/state");
    REQUIRE(server.mqttValue("test_switch/state") == "16");

    server.stop();
}

static const std::string multiple_converter = R"(
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
          converter: std.int32()
      state:
        register: tcptest.1.2
        register_type: holding
        count: 2
        converter: std.int32()
)";

TEST_CASE ("Write multiple registers with converter") {
    MockedModMqttServerThread server(multiple_converter);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 1);
    server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, 0);
    server.start();

    server.waitForPublish("test_switch/availability");
    REQUIRE(server.mqttValue("test_switch/availability") == "1");
    server.waitForPublish("test_switch/state");
    REQUIRE(server.mqttValue("test_switch/state") == "65536");

    server.publish("test_switch/set", "131073");

    server.waitForPublish("test_switch/state");
    REQUIRE(server.mqttValue("test_switch/state") == "131073");
    REQUIRE(server.mModbusFactory->getMockedModbusContext("tcptest").getWriteCount(1) == 1);

    server.stop();
}

