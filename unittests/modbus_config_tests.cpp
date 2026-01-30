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
      response_data_timeout: 500ms
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

    SECTION("should throw if response_timeout is outside range 0-999ms") {
        config.mYAML["modbus"]["networks"][0]["response_timeout"] = "1s";
        MockedModMqttServerThread server(config.toString(), false);
        server.start();
        server.stop();
        REQUIRE(server.initOk() == false);
    }

    SECTION("should throw if response_data_timeout is outside range 0-999ms") {
        config.mYAML["modbus"]["networks"][0]["response_data_timeout"] = "1s";
        MockedModMqttServerThread server(config.toString(), false);
        server.start();
        server.stop();
        REQUIRE(server.initOk() == false);
    }

    SECTION("should not throw if max value for response_timeout is set") {
        config.mYAML["modbus"]["networks"][0]["response_timeout"] = "999ms";
        MockedModMqttServerThread server(config.toString(), false);
        server.start();
        server.stop();
        REQUIRE(server.initOk() == true);
    }

    SECTION("should not throw if max value for response_data_timeout is set") {
        config.mYAML["modbus"]["networks"][0]["response_data_timeout"] = "999ms";
        MockedModMqttServerThread server(config.toString(), false);
        server.start();
        server.stop();
        REQUIRE(server.initOk() == true);
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
)");

    SECTION("should throw if register list is empty") {
        config.mYAML["mqtt"]["objects"][0]["state"]["registers"] = "";
        MockedModMqttServerThread server(config.toString(), false);
        server.start();
        server.stop();
        REQUIRE(server.initOk() == false);
    }

    SECTION("should throw if register map is empty") {
        config.mYAML["mqtt"]["objects"][0]["state"][0]["name"] = "empty map";
        MockedModMqttServerThread server(config.toString(), false);
        server.start();
        server.stop();
        REQUIRE(server.initOk() == false);
    }

}



TEST_CASE("Modbus exprtk configuration") {
TestConfig config(R"(
modmqttd:
  converter_search_path:
    - build/exprconv
  converter_plugins:
    - exprconv.so
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
      state:
        converter: expr.evaluate("R0 is not valid")
        register: tcptest.1.2
)");

    SECTION("should throw if expression is not valid") {
        MockedModMqttServerThread server(config.toString(), false);
        server.start();
        server.stop();
        REQUIRE(server.initOk() == false);
        const modmqttd::ConfigurationException& err(dynamic_cast<const modmqttd::ConfigurationException&>(server.getException()));
        REQUIRE(err.mLineNumber == 18);
    }
}
