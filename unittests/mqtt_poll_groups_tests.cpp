#include "catch2/catch_all.hpp"
#include "mockedserver.hpp"
#include "jsonutils.hpp"
#include "defaults.hpp"

static const std::string config1 = R"(
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
      slaves:
        - address: 1
          poll_groups:
            - register: 1
              register_type: input
              count: 10
            - register: 20
              register_type: input
              count: 5
mqtt:
  client_id: mqtt_test
  refresh: 1s
  broker:
    host: localhost
  objects:
    - topic: list_state
      state:
        converter: std.int32()
        registers:
          - register: tcptest.1.2
            register_type: input
          - register: tcptest.1.3
            register_type: input
    - topic: single_state
      state:
        register: tcptest.1.5
        register_type: input
    - topic: count_state
      state:
        converter: std.int32()
        register: tcptest.1.7
        register_type: input
        count: 2
    - topic: not_avail_state
      state:
        register: tcptest.1.20
        register_type: input
      availability:
        register: tcptest.1.22
        register_type: input
        available_value: 1
)";

TEST_CASE ("Topic state data should be pulled as pull group") {
    MockedModMqttServerThread server(config1);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 1);
    server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::INPUT, 1);

    server.setModbusRegisterValue("tcptest", 1, 5, modmqttd::RegisterType::INPUT, 5);

    server.setModbusRegisterValue("tcptest", 1, 7, modmqttd::RegisterType::INPUT, 2);
    server.setModbusRegisterValue("tcptest", 1, 8, modmqttd::RegisterType::INPUT, 1);

    server.setModbusRegisterValue("tcptest", 1, 20, modmqttd::RegisterType::INPUT, 20);
    server.setModbusRegisterValue("tcptest", 1, 22, modmqttd::RegisterType::INPUT, 0);

    server.start();

    server.waitForPublish("list_state/state");
    REQUIRE(server.mqttValue("list_state/state") == "65537");

    server.waitForPublish("not_avail_state/availability");
    REQUIRE(server.mqttValue("not_avail_state/availability") == "0");


    REQUIRE(server.mModbusFactory->getMockedModbusContext("tcptest").getReadCount(1) == 2);
    server.stop();
}

static const std::string config2 = R"(
modmqttd:
modbus:
  networks:
    - name: tcptest
      address: localhost
      port: 501
      slaves:
        - address: 1
          poll_groups:
            - register: 1
              register_type: input
              count: 2
mqtt:
  client_id: mqtt_test
  refresh: 10ms
  broker:
    host: localhost
  objects:
    - topic: first_state
      state:
        register: tcptest.1.1
        register_type: input
    - topic: second_state
      state:
        register: tcptest.1.2
        register_type: input
)";


TEST_CASE ("Unchanged state should not be published when pulled as a pull group") {
    MockedModMqttServerThread server(config2);
    server.setModbusRegisterValue("tcptest", 1, 1, modmqttd::RegisterType::INPUT, 1);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 2);

    server.start();

    // initial publish
    server.waitForPublish("first_state/state");

    server.setModbusRegisterValue("tcptest", 1, 1, modmqttd::RegisterType::INPUT, 10);

    server.waitForPublish("first_state/state");

    server.stop();

    server.requirePublishCount("second_state/state", 1);
}


TEST_CASE ("Availablity should be changed for all registers in poll group") {
    MockedModMqttServerThread server(config2);
    server.setModbusRegisterValue("tcptest", 1, 1, modmqttd::RegisterType::INPUT, 1);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 2);

    server.start();

    // initial publish
    server.waitForPublish("first_state/availability");
    REQUIRE(server.mqttValue("first_state/availability") == "1");
    server.waitForPublish("second_state/availability");
    REQUIRE(server.mqttValue("first_state/availability") == "1");

    server.setModbusRegisterValue("tcptest", 1, 1, modmqttd::RegisterType::INPUT, 10);
    server.setModbusRegisterReadError("tcptest", 1, 1, modmqttd::RegisterType::INPUT);

    //wait for 4sec for three read attempts
    server.waitForPublish("first_state/availability", defaultWaitTime(std::chrono::milliseconds(4000)));
    REQUIRE(server.mqttValue("first_state/availability") == "0");

    server.waitForPublish("second_state/availability", defaultWaitTime(std::chrono::milliseconds(4000)));
    REQUIRE(server.mqttValue("second_state/availability") == "0");

    server.stop();
}


static const std::string old_valid_pg_config = R"(
modmqttd:
modbus:
  networks:
    - name: tcptest
      address: localhost
      port: 501
      poll_groups:
        - register: 1.1
          register_type: input
          count: 2
mqtt:
  client_id: mqtt_test
  refresh: 10ms
  broker:
    host: localhost
  objects:
    - topic: first_state
      state:
        register: tcptest.1.1
        register_type: input
)";

TEST_CASE ("Old style poll groups should work as usual") {
    MockedModMqttServerThread server(old_valid_pg_config);
    server.setModbusRegisterValue("tcptest", 1, 1, modmqttd::RegisterType::INPUT, 1);

    server.start();

    // initial publish
    server.waitForPublish("first_state/state");
    REQUIRE(server.mqttValue("first_state/state") == "1");

    server.stop();
}

