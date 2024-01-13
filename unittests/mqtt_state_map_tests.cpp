#include "catch2/catch_all.hpp"
#include "mockedserver.hpp"
#include "jsonutils.hpp"
#include "defaults.hpp"

static const std::string config = R"(
modmqttd:
modbus:
  networks:
    - name: tcptest
      address: localhost
      port: 501
mqtt:
  client_id: mqtt_test
  refresh: 50ms
  broker:
    host: localhost
  objects:
    - topic: test_state
      state:
        - name: sensor1
          register: tcptest.1.2
          register_type: input
        - name: sensor2
          register: tcptest.1.3
          register_type: input
)";

TEST_CASE ("Named tuple should update if first component changes") {
    MockedModMqttServerThread server(config);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 1);
    server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::INPUT, 2);
    server.start();
    // default mocked modbus read time is 5ms per register
    server.waitForPublish("test_state/state");
    REQUIRE_JSON(server.mqttValue("test_state/state"), "{\"sensor1\": 1, \"sensor2\": 2}");

    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 10);
    //poll time is 50ms
    server.waitForPublish("test_state/state");
    REQUIRE_JSON(server.mqttValue("test_state/state"), "{\"sensor1\": 10, \"sensor2\": 2}");
    server.stop();
}
