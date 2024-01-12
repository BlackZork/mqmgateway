#include "catch2/catch_all.hpp"
#include "mockedserver.hpp"
#include "defaults.hpp"

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
    - topic: one
      state:
        register: tcptest.1.1
        register_type: input
    - topic: two
      state:
        register: tcptest.2.1
        register_type: input
)";

TEST_CASE ("two slaves with the same registers should publish two values") {
    MockedModMqttServerThread server(config);
    server.setModbusRegisterValue("tcptest", 1, 1, modmqttd::RegisterType::INPUT, 7);
    server.setModbusRegisterValue("tcptest", 2, 1, modmqttd::RegisterType::INPUT, 13);
    server.start();
    server.waitForPublish("one/state");
    REQUIRE(server.mqttValue("one/state") == "7");
    server.waitForPublish("two/state");
    REQUIRE(server.mqttValue("two/state") == "13");
    server.stop();
}
