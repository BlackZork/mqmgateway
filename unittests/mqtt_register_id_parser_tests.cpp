#include <catch2/catch_all.hpp>

#include "mockedserver.hpp"
#include "yaml_utils.hpp"

TEST_CASE ("Register id specified as hex should be parsed") {

TestConfig config(R"(
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
    - topic: test_sensor1
      network: tcptest
      slave: 1
      state:
        register: 0xA
        register_type: input
    - topic: test_sensor2
      network: tcptest
      slave: 1
      state:
        register: 0xf
        register_type: input
)");


    MockedModMqttServerThread server(config.toString());
    server.start();
    server.stop();
    return;
}
