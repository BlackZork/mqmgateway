#include "catch2/catch_all.hpp"
#include "mockedserver.hpp"
#include "jsonutils.hpp"
#include "defaults.hpp"

static const std::string config = R"(
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
        - name: sensor1
          register: tcptest.1.2
          register_type: input
          converter: std.divide(100,3)
        - name: sensor2
          register: tcptest.1.3
          register_type: input
          converter: std.divide(10,2)
)";

TEST_CASE ("Named map should output json map with converted values") {
    MockedModMqttServerThread server(config);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 1234);
    server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::INPUT, 100);
    server.start();
    // default mocked modbus read time is 5ms per register
    server.waitForPublish("test_state/state");
    REQUIRE_JSON(server.mqttValue("test_state/state"), "{\"sensor1\": 12.34, \"sensor2\": 10.0}");
    server.stop();
}
