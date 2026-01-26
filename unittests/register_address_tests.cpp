#include "catch2/catch_all.hpp"

#include "config.hpp"
#include "defaults.hpp"
#include "mockedserver.hpp"

using namespace Catch::Matchers;

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
        refresh: 2s
)";


    MockedModMqttServerThread server(config);
    server.start();

    server.waitForPublish("test_switch/state");
    server.publish("test_switch/set", "32");

    server.waitForPublish("test_switch/state", std::chrono::seconds(30));
    REQUIRE(server.mqttValue("test_switch/state") == "32");

    REQUIRE(server.getModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING) == 32);

    server.stop();
}

TEST_CASE ("decimal register address should not allow 0") {
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
      state:
        register: tcptest.1.0
        register_type: holding
        refresh: 2s
)";


    MockedModMqttServerThread server(config, false);
    server.start();
    server.stop();
    server.requireException<modmqttd::ConfigurationException>("Use hex address for 0-based");
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


TEST_CASE ("slave with id=0 should be allowed") {
static const std::string config = R"(
modbus:
  networks:
    - name: tcptest
      address: localhost
      port: 501
      slaves:
        - address: 0
          poll_groups:
            - register: 1
              register_type: input
              count: 10
            - register: 20
              register_type: input
              count: 5
mqtt:
  client_id: mqtt_test
  broker:
    host: localhost
  objects:
    - topic: test_switch
      network: tcptest
      slave: 0
      state:
        register: 1
        register_type: holding
    - topic: test_switch2
      network: tcptest
      state:
        register: 0.2
        register_type: holding
)";

    MockedModMqttServerThread server(config);
    // should not throw config exception
    server.start();
    server.stop();
}



