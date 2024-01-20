#include "catch2/catch_all.hpp"

#include "defaults.hpp"
#include "mockedserver.hpp"

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
      availability:
        register: tcptest.1.2
        register_type: bit
        available_value: 1
)";

    TEST_CASE ("Start and stop mocked server without throwing exceptions") {
        MockedModMqttServerThread server(config);
        server.start();
        server.stop();
    }

    TEST_CASE ("When modbus network is connected availability flag for registers should be set") {
        MockedModMqttServerThread server(config);
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::BIT, true);
        server.start();
        server.waitForPublish("test_switch/availability");
        REQUIRE(server.mqttValue("test_switch/availability") == "1");
        server.stop();
    }

    TEST_CASE ("mqtt availability should be unset for object with bad value in modbus availability register") {
        MockedModMqttServerThread server(config);
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::BIT, false);
        server.start();
        server.waitForPublish("test_switch/availability");
        REQUIRE(server.mqttValue("test_switch/availability") == "0");
        server.stop();
    }

    TEST_CASE ("After gateway shutdown mqtt availability flag should be unset") {
        MockedModMqttServerThread server(config);
        server.start();
        server.stop();
        REQUIRE(server.mqttValue("test_switch/availability") == "0");
    }

    TEST_CASE ("When modbus network is connected object state should be published immediately") {
        MockedModMqttServerThread server(config);
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::BIT, true);
        server.setModbusRegisterValue("tcptest", 1, 1, modmqttd::RegisterType::COIL, true);
        server.start();
        // default mocked modbus read time is 5ms per register
        server.waitForPublish("test_switch/state");
        REQUIRE(server.mqttValue("test_switch/state") == "1");
        server.stop();
    }

    TEST_CASE ("When registers cannot be read availability flag should be unset") {
        MockedModMqttServerThread server(config);

        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::BIT, true);
        server.start();
        server.waitForPublish("test_switch/availability");
        REQUIRE(server.mqttValue("test_switch/availability") == "1");

        server.disconnectModbusSlave("tcptest", 1);

        //max 5 sec for three register read attempts
        server.waitForPublish("test_switch/availability", std::chrono::seconds(5));
        REQUIRE(server.mqttValue("test_switch/availability") == "0");
        server.stop();
    }

    TEST_CASE ("If broker is restarted all mqtt objects should be republished") {
        MockedModMqttServerThread server(config);
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::BIT, true);
        server.setModbusRegisterValue("tcptest", 1, 1, modmqttd::RegisterType::COIL, true);
        server.start();

        server.waitForPublish("test_switch/state");
        REQUIRE(server.mqttValue("test_switch/state") == "1");
        server.waitForPublish("test_switch/availability");
        REQUIRE(server.mqttValue("test_switch/availability") == "1");

        server.mMqtt->resetBroker();

        server.waitForPublish("test_switch/state");
        REQUIRE(server.mqttValue("test_switch/state") == "1");
        server.waitForPublish("test_switch/availability");
        REQUIRE(server.mqttValue("test_switch/availability") == "1");

        server.stop();
    }
