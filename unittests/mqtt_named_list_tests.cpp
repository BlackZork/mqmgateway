#include "catch2/catch_all.hpp"
#include "mockedserver.hpp"
#include "jsonutils.hpp"
#include "yaml_utils.hpp"


TEST_CASE ("Named state list should output json map with list as value") {

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
        name: test_name
        registers:
          - register: tcptest.1.2
            register_type: input
          - register: tcptest.1.3
            register_type: input
)");

    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 1);
    server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::INPUT, 7);
    server.start();

    server.waitForPublish("test_state/availability");
    REQUIRE(server.mqttValue("test_state/availability") == "1");

    server.waitForPublish("test_state/state");

    REQUIRE_JSON(server.mqttValue("test_state/state"), "{ \"test_name\": [1,7] }");
    REQUIRE(server.mModbusFactory->getMockedModbusContext("tcptest").getReadCount(1) == 2);
    server.stop();
}


TEST_CASE ("Nested named state lists") {

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
    - topic: test_state
      state:
        name: parent_map
        registers:
          - name: child_map
            registers:
              - register: tcptest.1.2
                register_type: input
              - register: tcptest.1.3
                register_type: input
)");

SECTION("should output nested json map with list as value") {
    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 1);
    server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::INPUT, 7);
    server.start();

    server.waitForPublish("test_state/availability");
    REQUIRE(server.mqttValue("test_state/availability") == "1");

    server.waitForPublish("test_state/state");

    REQUIRE_JSON(server.mqttValue("test_state/state"), "{ \"parent_map\": { \"child_map\": [1,7]} }");
    REQUIRE(server.mModbusFactory->getMockedModbusContext("tcptest").getReadCount(1) == 2);
    server.stop();
}

SECTION("should output nested json map with single element list as value") {
    //leave tcptest.1.2 only
    config.mYAML["mqtt"]["objects"][0]["state"]["registers"][0]["registers"].remove(1);
    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 5);
    server.start();

    server.waitForPublish("test_state/availability");
    REQUIRE(server.mqttValue("test_state/availability") == "1");

    server.waitForPublish("test_state/state");

    REQUIRE_JSON(server.mqttValue("test_state/state"), "{ \"parent_map\": { \"child_map\": [5]} }");
    REQUIRE(server.mModbusFactory->getMockedModbusContext("tcptest").getReadCount(1) == 1);
    server.stop();
}


SECTION("should output converted value with for netsted list with converter") {
    //leave tcptest.1.2 only
    //config.mYAML["mqtt"]["objects"][0]["state"]["registers"][0]["registers"].remove(1);
    config.mYAML["mqtt"]["objects"][0]["state"]["registers"][0]["converter"] = "std.int32()";

    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 1);
    server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::INPUT, 7);
    server.start();

    server.waitForPublish("test_state/availability");
    REQUIRE(server.mqttValue("test_state/availability") == "1");

    server.waitForPublish("test_state/state");

    REQUIRE_JSON(server.mqttValue("test_state/state"), "{ \"parent_map\": { \"child_map\": 65543 } }");
    REQUIRE(server.mModbusFactory->getMockedModbusContext("tcptest").getReadCount(1) == 2);
    server.stop();
}


SECTION("should output converted value with for netsted single element list with converter") {
    //leave tcptest.1.2 only
    config.mYAML["mqtt"]["objects"][0]["state"]["registers"][0]["registers"].remove(1);
    config.mYAML["mqtt"]["objects"][0]["state"]["registers"][0]["converter"] = "std.int32()";

    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 5);
    server.start();

    server.waitForPublish("test_state/availability");
    REQUIRE(server.mqttValue("test_state/availability") == "1");

    server.waitForPublish("test_state/state");

    REQUIRE_JSON(server.mqttValue("test_state/state"), "{ \"parent_map\": { \"child_map\": 5 } }");
    REQUIRE(server.mModbusFactory->getMockedModbusContext("tcptest").getReadCount(1) == 1);
    server.stop();
}



}


TEST_CASE ("Nested named state lists should output nested json map with converted value") {

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
    - topic: test_state
      state:
        name: parent_map
        registers:
          - name: child_map
            register: tcptest.1.2
            count: 2
            converter: std.int32()
)");

    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 2);
    server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, 1);
    server.start();

    server.waitForPublish("test_state/availability");
    REQUIRE(server.mqttValue("test_state/availability") == "1");

    server.waitForPublish("test_state/state");

    REQUIRE_JSON(server.mqttValue("test_state/state"), "{ \"parent_map\": { \"child_map\": 131073} }");
    REQUIRE(server.mModbusFactory->getMockedModbusContext("tcptest").getReadCount(1) == 1);
    server.stop();
}
