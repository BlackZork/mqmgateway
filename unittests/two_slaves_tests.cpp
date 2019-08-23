#include "catch2/catch.hpp"

#include "mockedserver.hpp"

//make sure that nothing listen on given ports
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
    - topic: one
      state:
        register: tcptest.1.1
        register_type: input
    - topic: two
      state:
        register: tcptest.2.1
        register_type: input
)";

    TEST_CASE ("two slaves with the same registers should publish two values") {
        MockedModMqttServerThread server(config);
        server.setModbusRegisterValue("tcptest", 1, 1, modmqttd::RegisterType::INPUT, 7);
        server.setModbusRegisterValue("tcptest", 2, 1, modmqttd::RegisterType::INPUT, 13);
        server.start();
        server.waitForPublish("one/state", std::chrono::milliseconds(300));
        REQUIRE(server.mqttValue("one/state") == "7");
        server.waitForPublish("two/state", std::chrono::milliseconds(300));
        REQUIRE(server.mqttValue("two/state") == "13");
        server.stop();
    }


    //TODO TEST_CASE for order: value first, availablity then
