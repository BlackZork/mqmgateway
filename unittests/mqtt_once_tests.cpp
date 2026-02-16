#include "catch2/catch_all.hpp"
#include "mockedserver.hpp"
#include "defaults.hpp"
#include "yaml_utils.hpp"

TEST_CASE ("once should not publish state if register is read by another topic") {

TestConfig config(R"(
modbus:
  networks:
    - name: tcptest
      address: localhost
      port: 501
mqtt:
  client_id: mqtt_test
  refresh: 10ms
  broker:
    host: localhost
  objects:
    - topic: refreshing_topic
      publish_mode: on_change
      state:
        register: tcptest.1.2
    - topic: once_topic
      publish_mode: once
      state:
        register: tcptest.1.2
)");

    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 1);
    server.start();

    server.waitForPublish("refreshing_topic/state");
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 2);
    server.waitForPublish("refreshing_topic/state");
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 3);
    server.waitForPublish("refreshing_topic/state");

    server.stop();
    server.requirePublishCount("once_topic/state", 1);
}

//TODO 2-3 read errors before first successful read, availability check
