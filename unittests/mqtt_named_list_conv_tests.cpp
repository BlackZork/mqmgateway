#include "catch2/catch.hpp"
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
        name: some_name
        converter: std.int32()
        registers:
          - register: tcptest.1.2
            register_type: input
          - register: tcptest.1.3
            register_type: input
)";

TEST_CASE ("Named state list should output converted value") {
    MockedModMqttServerThread server(config);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 2);
    server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::INPUT, 1);
    server.start();

    //to make sure that all registers have initial value
    server.waitForPublish("test_state/availability");
    REQUIRE(server.mqttValue("test_state/availability") == "1");

    server.waitForPublish("test_state/state");

    //2^17 + 1
    REQUIRE_JSON(server.mqttValue("test_state/state"), "{\"some_name\": 131073}");
    REQUIRE(server.mModbusFactory->getMockedModbusContext("tcptest").getReadCount(1) == 1);
    server.stop();
}
