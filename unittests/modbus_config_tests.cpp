#include "catch2/catch_all.hpp"
#include "mockedserver.hpp"
#include "defaults.hpp"
#include "yaml_utils.hpp"


TEST_CASE("Modbus timeout configuration") {
TestConfig config(R"(
modbus:
  networks:
    - name: tcptest
      address: localhost
      response_timeout: 500ms
      response_data_timeout: 500s
      port: 501
      slaves:
        - address: 1
          delay_before_command: 15ms
mqtt:
  client_id: mqtt_test
  broker:
    host: localhost
  objects:
    - topic: test_sensor
      state:
        register: tcptest.1.2
)");

    SECTION("shoud throw if response_timeout is outside range 0-999ms") {
        config.mYAML["modbus"]["networks"][0]["response_timeout"] = "1s";
        MockedModMqttServerThread server(config.toString(), false);
        server.start();
        server.stop();
        REQUIRE(server.initOk() == false);
    }

    SECTION("shoud throw if response_data_timeout is outside range 0-999ms") {
        config.mYAML["modbus"]["networks"][0]["response_data_timeout"] = "1s";
        MockedModMqttServerThread server(config.toString(), false);
        server.start();
        server.stop();
        REQUIRE(server.initOk() == false);
    }
}


TEST_CASE("Modbus state configuration") {
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
    - topic: invalid
      state:
        registers:
)");

    SECTION("shoud throw if register list is empty") {
        MockedModMqttServerThread server(config.toString(), false);
        server.start();
        server.stop();
        REQUIRE(server.initOk() == false);
    }


}

