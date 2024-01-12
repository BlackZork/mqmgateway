#include "catch2/catch_all.hpp"
#include "mockedserver.hpp"
#include "defaults.hpp"

static const std::string config = R"(
modmqttd:
  converter_search_path:
    - build/exprconv
  converter_plugins:
    - exprconv.so
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
        converter: expr.evaluate("R0 / R1", 3)
        registers:
          - register: tcptest.1.2
            register_type: input
          - register: tcptest.1.3
            register_type: input
)";

TEST_CASE ("Expression on list should evaluate") {
    MockedModMqttServerThread server(config);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 10);
    server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::INPUT, 3);
    server.start();

    //to make sure that all registers have initial value
    server.waitForPublish("test_state/availability");
    REQUIRE(server.mqttValue("test_state/availability") == "1");

    server.waitForPublish("test_state/state");

    REQUIRE(server.mqttValue("test_state/state") == "3.333");
    server.stop();
}
