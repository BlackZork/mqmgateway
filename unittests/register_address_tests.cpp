#include "catch2/catch_all.hpp"

#include "defaults.hpp"
#include "mockedserver.hpp"

TEST_CASE ("decimal register address should start from 1") {
static const std::string config = R"(
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
    - topic: test_switch
      commands:
        - name: set
          register: tcptest.1.2
          register_type: holding
      state:
        register: tcptest.1.2
        register_type: holding
)";


    MockedModMqttServerThread server(config);
    server.start();

    server.waitForPublish("test_switch/state");
    server.publish("test_switch/set", "32");

    server.waitForPublish("test_switch/state");
    REQUIRE(server.mqttValue("test_switch/state") == "32");

    std::shared_ptr<modmqttd::IModbusContext> ctx = server.mModbusFactory->getContext("tcptest");
    MockedModbusContext& c = static_cast<MockedModbusContext&>(*ctx);
    MockedModbusContext::Slave& s = c.getSlave(1);
    REQUIRE(s.mHolding.find(1) != s.mHolding.end());
    REQUIRE(s.mHolding[1].mValue == 32);

    server.stop();
}

TEST_CASE ("hex register address should start from 0") {
static const std::string config = R"(
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
    - topic: test_switch
      commands:
        - name: set
          register: tcptest.1.0x0
          register_type: holding
      state:
        register: tcptest.1.0x0
        register_type: holding
)";


    MockedModMqttServerThread server(config);
    server.start();

    server.waitForPublish("test_switch/state");
    server.publish("test_switch/set", "32");

    server.waitForPublish("test_switch/state");
    REQUIRE(server.mqttValue("test_switch/state") == "32");

    std::shared_ptr<modmqttd::IModbusContext> ctx = server.mModbusFactory->getContext("tcptest");
    MockedModbusContext& c = static_cast<MockedModbusContext&>(*ctx);
    MockedModbusContext::Slave& s = c.getSlave(1);
    REQUIRE(s.mHolding.find(0) != s.mHolding.end());
    REQUIRE(s.mHolding[0].mValue == 32);

    server.stop();
}


