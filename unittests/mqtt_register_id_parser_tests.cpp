#include "catch2/catch_all.hpp"
#include "mockedserver.hpp"
#include "defaults.hpp"

#include <yaml-cpp/yaml.h>

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
    - topic: test_sensor
      network: tcptest
      slave: 1
      state:
        register: 0xA
        register_type: input
    - topic: test_sensor
      network: tcptest
      slave: 1
      state:
        register: 0xf
        register_type: input
)";


TEST_CASE ("Register id specified as hex should be parsed") {
    MockedModMqttServerThread server(config);
    server.start();
    server.stop();
    return;
}
