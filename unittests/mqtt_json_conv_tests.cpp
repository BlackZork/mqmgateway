#include "catch2/catch_all.hpp"
#include "mockedserver.hpp"
#include "jsonutils.hpp"
#include "yaml_utils.hpp"

TEST_CASE ("JSON value converted with") {

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
  refresh: 1s
  broker:
    host: localhost
  objects:
    - topic: test_state
      state:
        - register: tcptest.1.2
          converter: std.multiply(10)
)");

SECTION ("std.multiply and no precision should output int value") {
    config.mYAML["mqtt"]["objects"][0]["state"][0]["converter"] = "std.multiply(10)";

    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 8);
    server.start();

    server.waitForPublish("test_state/state");

    REQUIRE_JSON(server.mqttValue("test_state/state"), "[80]");
    server.stop();
}

SECTION ("std.multiply and precision should output double value") {
    config.mYAML["mqtt"]["objects"][0]["state"][0]["converter"] = "std.multiply(10,2)";

    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 8);
    server.start();

    server.waitForPublish("test_state/state");

    REQUIRE_JSON(server.mqttValue("test_state/state"), "[80.00]");
    server.stop();
}

SECTION ("std.divide and no precision should output six fractional digits") {
    config.mYAML["mqtt"]["objects"][0]["state"][0]["converter"] = "std.divide(3)";

    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 1);
    server.start();

    server.waitForPublish("test_state/state");

    REQUIRE_JSON(server.mqttValue("test_state/state"), "[0.333333]");
    server.stop();
}

SECTION ("std.divide should output fractional digits set with precision arg") {
    config.mYAML["mqtt"]["objects"][0]["state"][0]["converter"] = "std.divide(3,2)";

    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 1);
    server.start();

    server.waitForPublish("test_state/state");

    REQUIRE_JSON(server.mqttValue("test_state/state"), "[0.33]");
    server.stop();
}

SECTION ("std.divide should output fractional digits set with precision arg") {
    config.mYAML["mqtt"]["objects"][0]["state"][0]["converter"] = "std.divide(3,0)";

    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 10);
    server.start();

    server.waitForPublish("test_state/state");

    REQUIRE_JSON(server.mqttValue("test_state/state"), "[3]");
    server.stop();
}



}
