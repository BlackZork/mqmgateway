#include "catch2/catch.hpp"
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
    - topic: test_sensor
      state:
        register: tcptest.1.2
        register_type: input
        converter: expr.evaluate("R0 - 65536")
)";

TEST_CASE ("Expression on unnamed scalar should be evaluated") {
    MockedModMqttServerThread server(config);
    int16_t negative = -20;
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, negative);
    server.start();
    // default mocked modbus read time is 5ms per register
    server.waitForPublish("test_sensor/state");
    REQUIRE(server.mqttValue("test_sensor/state") == "-20");
    server.stop();
}
