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
    - topic: test_sensor
      state:
        name: some_name
        register: tcptest.1.2
        register_type: input
        converter: std.divide(10,3)
)";

TEST_CASE ("Named scalar should output converted value") {
    MockedModMqttServerThread server(config);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 32456);
    server.start();
    // default mocked modbus read time is 5ms per register
    server.waitForPublish("test_sensor/state");
    REQUIRE_JSON(server.mqttValue("test_sensor/state"), "{ \"some_name\": 3245.6}");
    server.stop();
}
