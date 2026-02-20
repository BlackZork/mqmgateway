#include "catch2/catch_all.hpp"
#include "mockedserver.hpp"
#include "yaml_utils.hpp"

TEST_CASE ("Unnamed scalar should output raw value") {

TestConfig config(R"(
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
)");

    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 32456);
    server.start();
    // default mocked modbus read time is 5ms per register
    server.waitForPublish("test_sensor/state");
    REQUIRE(server.mqttValue("test_sensor/state") == "32456");
    server.stop();
}

