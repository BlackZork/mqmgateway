#include "catch2/catch_all.hpp"
#include "mockedserver.hpp"
#include "defaults.hpp"

static const std::string config = R"(
modmqttd:
modbus:
  networks:
    - name: tcptest
      address: localhost
      port: 501
      delay_before_command: 500ms
mqtt:
  client_id: mqtt_test
  broker:
    host: localhost
  objects:
    - topic: test_switch
      commands:
        - name: set
          register: tcptest.1.2
          register_type: holding
)";
TEST_CASE ("Write value in write-only configuration should succeed") {
    MockedModMqttServerThread server(config);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 0);
    server.start();

    server.waitForSubscription("test_switch/set");

    server.publish("test_switch/set", "7");
    server.waitForModbusValue("tcptest",1,2, modmqttd::RegisterType::HOLDING, 0x7);

    server.stop();
}


TEST_CASE ("Write should respect delay_before_command") {
    MockedModMqttServerThread server(config);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 0);
    server.start();

    server.waitForSubscription("test_switch/set");

    auto now = std::chrono::steady_clock::now();
    server.publish("test_switch/set", "7");
    server.publish("test_switch/set", "8");
    server.waitForModbusValue("tcptest",1,2, modmqttd::RegisterType::HOLDING, 0x8, std::chrono::milliseconds(700));
    auto after = std::chrono::steady_clock::now();
    REQUIRE(after - now >= std::chrono::milliseconds(500));

    server.stop();
}
