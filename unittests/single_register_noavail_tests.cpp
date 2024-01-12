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
  refresh: 1s
  broker:
    host: localhost
  objects:
    - topic: test_switch
      state:
        register: tcptest.1.1
        register_type: coil
)";

    TEST_CASE ("Availability flag for noavail register should be set after first read") {
        MockedModMqttServerThread server(config);
        server.start();
        server.waitForPublish("test_switch/state");
        REQUIRE(server.mqttValue("test_switch/state") == "0");
        server.waitForPublish("test_switch/availability");
        REQUIRE(server.mqttValue("test_switch/availability") == "1");
        server.stop();
    }

    TEST_CASE ("After gateway shutdown noavail register availability flag should be unset") {
        MockedModMqttServerThread server(config);
        server.start();
        server.stop();
        REQUIRE(server.mqttValue("test_switch/availability") == "0");
    }

    TEST_CASE ("When noavail registers cannot be read availability flag should be unset") {
        MockedModMqttServerThread server(config);

        server.start();
        server.waitForPublish("test_switch/availability");
        REQUIRE(server.mqttValue("test_switch/availability") == "1");

        server.disconnectModbusSlave("tcptest", 1);

        //max 4 sec for three register read attempts
        server.waitForPublish("test_switch/availability", defaultWaitTime(std::chrono::seconds(5)));
        REQUIRE(server.mqttValue("test_switch/availability") == "0");
        server.stop();
    }

    TEST_CASE ("Value should be set before availability") {
        MockedModMqttServerThread server(config);
        server.setModbusRegisterValue("tcptest", 1, 1, modmqttd::RegisterType::BIT, true);
        server.start();
        std::string topic = server.waitForFirstPublish();
        REQUIRE("test_switch/state" == topic);

        server.stop();
    }

//TODO TEST_CASE for order: value first, availablity then
