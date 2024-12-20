#include "catch2/catch_all.hpp"
#include "mockedserver.hpp"
#include "defaults.hpp"
#include "yaml_utils.hpp"

TEST_CASE ("When retain") {

TestConfig config(R"(
modbus:
  networks:
    - name: tcptest
      address: localhost
      port: 501
mqtt:
  client_id: mqtt_test
  refresh: 100ms
  broker:
    host: localhost
  objects:
    - topic: test_sensor
      state:
        register: tcptest.1.2
    - topic: other_sensor
      state:
        register: tcptest.1.3
)");
    SECTION("is not defined then initial poll do trigger publish") {
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 1);
        server.start();
        server.waitForPublish("test_sensor/state");
        REQUIRE(server.mqttValue("test_sensor/state") == "1");
    }

    SECTION("is set to false then initial poll do trigger publish") {
        config.mYAML["mqtt"]["objects"][0]["retain"] = "false";
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 1);
        server.start();
        server.waitForPublish("test_sensor/state");
        REQUIRE(server.mqttValue("test_sensor/state") == "1");
    }

    SECTION("is set to true then initial poll do not trigger publish") {
        config.mYAML["mqtt"]["objects"][0]["retain"] = "true";
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 1);
        server.start();
        server.waitForInitialPoll("tcptest");
        server.requirePublishCount("test_sensor/state", 0);

        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 2);
        server.waitForPublish("test_sensor/state");
        REQUIRE(server.mqttValue("test_sensor/state") == "2");
    }
}

