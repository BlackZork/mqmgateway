#include <catch2/catch_all.hpp>
#include "mockedserver.hpp"
#include "yaml_utils.hpp"

TEST_CASE ("Parse register id with default slave and network name") {
TestConfig config(R"(
modbus:
  networks:
    - name: tcptest
      address: localhost
      port: 501
mqtt:
  client_id: mqtt_test
  broker:
    host: localhost
  objects:
    - topic: test_sensor
      network: tcptest
      slave: 1
      state:
        register: 2
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

