#include "catch2/catch.hpp"
#include "mockedserver.hpp"

//make sure that nothing listen on given ports
static const std::string config = R"(
modmqttd:
  loglevel: 5
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
    - topic: test_sensor
      state:
        register: tcptest.1.2
        register_type: input
        converter: std.divide(1000,3)
)";

TEST_CASE ("Unnamed scalar should output converted value") {
    MockedModMqttServerThread server(config);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 32456);
    server.start();
    // default mocked modbus read time is 5ms per register
    server.waitForPublish("test_sensor/state", std::chrono::milliseconds(30));
    REQUIRE(server.mqttValue("test_sensor/state") == "32.456");
    server.stop();
}
