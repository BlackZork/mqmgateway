#include "catch2/catch.hpp"

#include "mockedserver.hpp"
#include "defaults.hpp"

static const std::string config = R"(
modmqttd:
  templates_dir: ./test_templates
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
      template: simple_device
      network: tcptest
      slave: 1
)";

TEST_CASE ("Simple device template should work") {
    MockedModMqttServerThread server(config);

    server.setModbusRegisterValue("tcptest", 1, 1, modmqttd::RegisterType::COIL, 1);
    server.start();
    // default mocked modbus read time is 5ms per register
    server.waitForPublish("test_switch/state", REGWAIT_MSEC);
    REQUIRE(server.mqttValue("test_switch/state") == "1");
    server.stop();
}

