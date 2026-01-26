#include "catch2/catch_all.hpp"
#include "mockedserver.hpp"
#include "jsonutils.hpp"
#include "defaults.hpp"

TEST_CASE ("Named map should output json map with converted values") {

const std::string config = R"(
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
          converter: std.divide(100,precision=3)
        - name: sensor2
          register: tcptest.1.3
          register_type: input
          converter: std.divide(10,precision=2)
)";


    MockedModMqttServerThread server(config);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 1234);
    server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::INPUT, 100);
    server.start();
    server.waitForPublish("test_state/state");
    REQUIRE_JSON(server.mqttValue("test_state/state"), "{\"sensor1\": 12.34, \"sensor2\": 10.0}");
    server.stop();
}


TEST_CASE ("Named map should output json map with sublist converted to scalars") {

const std::string config = R"(
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
          count: 2
          converter: std.int32()
        - name: sensor2
          register: tcptest.2.4
          register_type: input
          count: 2
          converter: std.int32()
)";

    MockedModMqttServerThread server(config);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 1);
    server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::INPUT, 1);
    server.setModbusRegisterValue("tcptest", 2, 4, modmqttd::RegisterType::INPUT, 2);
    server.setModbusRegisterValue("tcptest", 2, 5, modmqttd::RegisterType::INPUT, 2);
    server.start();
    server.waitForPublish("test_state/state");
    REQUIRE_JSON(server.mqttValue("test_state/state"), "{\"sensor1\": 65537, \"sensor2\": 131074}");
    server.stop();
}
