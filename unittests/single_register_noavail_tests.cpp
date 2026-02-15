#include "catch2/catch_all.hpp"

#include "mockedserver.hpp"
#include "defaults.hpp"

TEST_CASE ("Availability flag") {

std::string config = R"(
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

    SECTION("for noavail register should be set after first read") {
        MockedModMqttServerThread server(config);
        server.start();
        server.waitForPublish("test_switch/state");
        REQUIRE(server.mqttValue("test_switch/state") == "0");
        server.waitForPublish("test_switch/availability");
        REQUIRE(server.mqttValue("test_switch/availability") == "1");
        server.stop();
    }

    SECTION ("after shutdown should be unset") {
        MockedModMqttServerThread server(config);
        server.start();
        server.stop();
        REQUIRE(server.mqttValue("test_switch/availability") == "0");
    }

    SECTION ("should be unset if register cannot be read") {
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

    SECTION ("should be publised after state") {
        MockedModMqttServerThread server(config);
        server.setModbusRegisterValue("tcptest", 1, 1, modmqttd::RegisterType::BIT, true);
        server.start();
        std::string topic = server.waitForFirstPublish();
        REQUIRE("test_switch/state" == topic);

        server.stop();
    }


    SECTION ("should be set after reconnect") {
        MockedModMqttServerThread server(config);
        server.setModbusRegisterValue("tcptest", 1, 1, modmqttd::RegisterType::COIL, false);

        server.start();

        server.waitForPublish("test_switch/availability");
        server.waitForPublish("test_switch/state");
        REQUIRE(server.mqttValue("test_switch/state") == "0");
        REQUIRE(server.mqttValue("test_switch/availability") == "1");

        server.disconnectModbusSlave("tcptest", 1);

        server.waitForPublish("test_switch/availability", std::chrono::seconds(5));
        REQUIRE(server.mqttValue("test_switch/availability") == "0");

        //server.setModbusRegisterValue("tcptest", 1, 1, modmqttd::RegisterType::COIL, true);
        server.connectModbusSlave("tcptest", 1);

        std::string topic = server.waitForFirstPublish(std::chrono::seconds(5));
        REQUIRE("test_switch/state" == topic);

        server.waitForPublish("test_switch/availability", std::chrono::seconds(5));
        REQUIRE(server.mqttValue("test_switch/availability") == "1");

        server.stop();
    }

}
