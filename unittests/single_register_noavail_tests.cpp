#include "catch2/catch.hpp"

#include "mockedserver.hpp"

//make sure that nothing listen on given ports
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
        server.waitForPublish("test_switch/state", std::chrono::seconds(1000));
        REQUIRE(server.mqttValue("test_switch/state") == "0");
        server.waitForPublish("test_switch/availability", std::chrono::seconds(1000));
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
        server.waitForPublish("test_switch/availability", std::chrono::seconds(10));
        REQUIRE(server.mqttValue("test_switch/availability") == "1");

        server.disconnectModbusSlave("tcptest", 1);

        //max 4 sec for three register read attempts
        server.waitForPublish("test_switch/availability", std::chrono::seconds(5));
        REQUIRE(server.mqttValue("test_switch/availability") == "0");
        server.stop();
    }
