#include "catch2/catch.hpp"
#include "mockedserver.hpp"
#include "jsonutils.hpp"

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
    - topic: test_state
      state:
        - register: tcptest.1.2
          register_type: input
        - register: tcptest.1.3
          register_type: input
)";

TEST_CASE ("Unnamed state list without register value should not be available") {
    MockedModMqttServerThread server(config);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 1);
    server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::INPUT, 7);
    server.setModbusRegisterReadError("tcptest", 1, 3, modmqttd::RegisterType::INPUT);

    server.start();

    //wait for 4sec for three read attempts
    server.waitForPublish("test_state/availability", std::chrono::milliseconds(4000));
    REQUIRE(server.mqttValue("test_state/availability") == "0");
    server.stop();
}


TEST_CASE ("Unnamed state list should output json array") {
    MockedModMqttServerThread server(config);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 1);
    server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::INPUT, 7);
    server.start();

    //to make sure that all registers have initial value
    server.waitForPublish("test_state/availability", std::chrono::milliseconds(30));
    REQUIRE(server.mqttValue("test_state/availability") == "1");

    server.waitForPublish("test_state/state", std::chrono::milliseconds(30));

    REQUIRE_JSON(server.mqttValue("test_state/state"), "[1,7]");
    server.stop();
}
