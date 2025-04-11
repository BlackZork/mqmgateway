#include "catch2/catch_all.hpp"
#include "mockedserver.hpp"
#include "jsonutils.hpp"
#include "defaults.hpp"
#include "yaml_utils.hpp"


TEST_CASE ("Unnamed state list should output converted value") {

static const std::string config = R"(
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
        converter: std.int32()
        registers:
          - register: tcptest.1.2
            register_type: input
          - register: tcptest.1.3
            register_type: input
)";

    MockedModMqttServerThread server(config);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 1);
    server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::INPUT, 1);
    server.start();

    //to make sure that all registers have initial value
    server.waitForPublish("test_state/availability");
    REQUIRE(server.mqttValue("test_state/availability") == "1");

    server.waitForPublish("test_state/state");

    REQUIRE(server.mModbusFactory->getMockedModbusContext("tcptest").getReadCount(1) == 2);
    REQUIRE(server.mqttValue("test_state/state") == "65537");
    server.stop();
}

TEST_CASE ("Unnamed state list should output converted value issuing single modbus read call") {

static const std::string config = R"(
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
        converter: std.int32()
        register: tcptest.1.2
        register_type: input
        count: 2
)";

    MockedModMqttServerThread server(config);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 1);
    server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::INPUT, 1);
    server.start();

    //to make sure that all registers have initial value
    server.waitForPublish("test_state/availability");
    REQUIRE(server.mqttValue("test_state/availability") == "1");

    server.waitForPublish("test_state/state");

    REQUIRE(server.mModbusFactory->getMockedModbusContext("tcptest").getReadCount(1) == 1);
    REQUIRE(server.mqttValue("test_state/state") == "65537");
    server.stop();
}

TEST_CASE ("Unnamed state list with two element child lists") {

static const std::string config = R"(
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
          count: 2
          converter: std.int32()
        - register: tcptest.1.4
          count: 2
          converter: std.int32()
)";

    SECTION ("should output converted value") {
        MockedModMqttServerThread server(config);
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 1);
        server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, 0);
        server.setModbusRegisterValue("tcptest", 1, 4, modmqttd::RegisterType::HOLDING, 0);
        server.setModbusRegisterValue("tcptest", 1, 5, modmqttd::RegisterType::HOLDING, 1);
        server.start();

        //to make sure that all registers have initial value
        server.waitForPublish("test_state/availability");
        REQUIRE(server.mqttValue("test_state/availability") == "1");

        server.waitForPublish("test_state/state");

        REQUIRE(server.mModbusFactory->getMockedModbusContext("tcptest").getReadCount(1) == 2);
        REQUIRE_JSON(server.mqttValue("test_state/state"), "[65536, 1]");
        server.stop();
    }
}

