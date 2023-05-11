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

// Better stack print in case of failure
#define REQUIRE_PUBLISH(server, topic, timeout) REQUIRE(server.checkForPublish(topic, timeout) == true)

TEST_CASE ("Holding register valid write") {
    MockedModMqttServerThread server(config);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 0);
    server.start();

    REQUIRE_PUBLISH(server, "test_switch/availability", REGWAIT_MSEC);
    REQUIRE(server.mqttValue("test_switch/availability") == "1");
    REQUIRE_PUBLISH(server, "test_switch/state", REGWAIT_MSEC);

    server.publish("test_switch/set", "32");

    REQUIRE_PUBLISH(server, "test_switch/state", REGWAIT_MSEC);
    REQUIRE(server.mqttValue("test_switch/state") == "32");

    server.stop();
}

TEST_CASE ("Holding register valid write, negative value are invalid") {
    MockedModMqttServerThread server(config);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 42);
    server.start();

    server.waitForPublish("test_switch/availability", REGWAIT_MSEC);
    REQUIRE(server.mqttValue("test_switch/availability") == "1");
    server.waitForPublish("test_switch/state", REGWAIT_MSEC);

    server.publish("test_switch/set", "-32");

    REQUIRE(server.mMqtt->waitForPublish("test_switch/state", REGWAIT_MSEC) == false);
    REQUIRE(server.getModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING) == 42);

    server.stop();
}

TEST_CASE ("Coil register valid write") {
    MockedModMqttServerThread server(config);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::COIL, 0);
    server.start();

    REQUIRE_PUBLISH(server, "test_switch/availability", REGWAIT_MSEC);
    REQUIRE(server.mqttValue("test_switch/availability") == "1");
    REQUIRE_PUBLISH(server, "test_switch/state", REGWAIT_MSEC);

    server.publish("test_switch/set", "1");
    REQUIRE_PUBLISH(server, "test_switch/state", REGWAIT_MSEC);
    REQUIRE(server.mqttValue("test_switch/state") == "1");

    server.stop();
}

TEST_CASE ("Mqtt invalid value should not crash server") {
    MockedModMqttServerThread server(config);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::COIL, 0);
    server.start();

    REQUIRE_PUBLISH(server, "test_switch/availability", REGWAIT_MSEC);
    REQUIRE(server.mqttValue("test_switch/availability") == "1");
    REQUIRE_PUBLISH(server, "test_switch/state", REGWAIT_MSEC);

    server.publish("test_switch/set", "1");
    REQUIRE_PUBLISH(server, "test_switch/state", REGWAIT_MSEC);
    REQUIRE(server.mqttValue("test_switch/state") == "1");

    server.publish("test_switch/set", "hello, world!");
    REQUIRE(server.checkForPublish("test_switch/state", REGWAIT_MSEC) == false);
    REQUIRE(server.mqttValue("test_switch/state") == "1");

    server.stop();
}

TEST_CASE ("Mqtt register range value should not be permitted if not configured") {
    MockedModMqttServerThread server(config);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 42);
    server.start();

    REQUIRE_PUBLISH(server, "test_switch/availability", REGWAIT_MSEC);
    REQUIRE(server.mqttValue("test_switch/availability") == "1");
    REQUIRE_PUBLISH(server, "test_switch/state", REGWAIT_MSEC);

    server.publish("test_switch/set", "43 44");
    REQUIRE(server.checkForPublish("test_switch/state", REGWAIT_MSEC) == false);

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
TEST_CASE ("Holding register valid write to subpath topic") {
    MockedModMqttServerThread server(config_subpath);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 0);
    server.start();

    REQUIRE_PUBLISH(server, "some/subpath/test_switch/availability", REGWAIT_MSEC);
    REQUIRE(server.mqttValue("some/subpath/test_switch/availability") == "1");
    REQUIRE_PUBLISH(server, "some/subpath/test_switch/state", REGWAIT_MSEC);

    server.publish("some/subpath/test_switch/set", "32");

    REQUIRE_PUBLISH(server, "some/subpath/test_switch/state", REGWAIT_MSEC);
    REQUIRE(server.mqttValue("some/subpath/test_switch/state") == "32");

    server.stop();
}
