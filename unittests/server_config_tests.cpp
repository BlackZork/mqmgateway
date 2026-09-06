#include "catch2/catch_all.hpp"

#include "config.hpp"
#include "yaml_utils.hpp"
#include "mockedserver.hpp"

TEST_CASE("Server configuration") {
TestConfig config(R"(
modmqttd:

modbus:
  networks:
    - name: tcptest
      address: localhost
      port: 64555
mqtt:
  client_id: mqtt_test
  broker:
    host: localhost
  objects:
    - topic: test_sensor
      state:
        register: tcptest.1.2
)");

    SECTION("with invalid log level name should throw config exception") {
        config.mYAML["modmqttd"]["log_level"] = "invalid";

        MockedModMqttServerThread server(config.toString(), false);
        server.start();
        server.stop();
        REQUIRE(server.initOk() == false);
        const modmqttd::ConfigurationException& err(dynamic_cast<const modmqttd::ConfigurationException&>(server.getException()));
        REQUIRE(err.mLineNumber == 2);
    }

    SECTION("with invalid log level number should throw config exception") {
        config.mYAML["modmqttd"]["log_level"] = "7";

        MockedModMqttServerThread server(config.toString(), false);
        server.start();
        server.stop();
        REQUIRE(server.initOk() == false);
        const modmqttd::ConfigurationException& err(dynamic_cast<const modmqttd::ConfigurationException&>(server.getException()));
        REQUIRE(err.mLineNumber == 2);
    }

    SECTION("with valid log level name") {
        config.mYAML["modmqttd"]["log_level"] = "error";

        MockedModMqttServerThread server(config.toString(), false);
        server.start();
        server.stop();
        REQUIRE(server.initOk() == true);
        REQUIRE(spdlog::get_level() == spdlog::level::err);
    }

    SECTION("with valid log level number") {
        config.mYAML["modmqttd"]["log_level"] = "5";

        MockedModMqttServerThread server(config.toString(), false);
        server.start();
        server.stop();
        REQUIRE(server.initOk() == true);
        REQUIRE(spdlog::get_level() == spdlog::level::debug);
    }

}

TEST_CASE("Converter plugin") {
    TestConfig config(R"(
modmqttd:
  converter_search_path:
    - abiconv
  converter_plugins:
    - badabiconv.so
modbus:
  networks:
    - name: tcptest
      address: localhost
      port: 64555
mqtt:
  client_id: mqtt_test
  broker:
    host: localhost
  objects:
    - topic: test_sensor
      state:
        register: tcptest.1.2
)");

    SECTION("built for a different ABI version should be refused") {
        MockedModMqttServerThread server(config.toString(), false);
        server.start();
        server.stop();

        REQUIRE(server.initOk() == false);
        server.requireException<modmqttd::ConfigurationException>("converter ABI version");
    }

    SECTION("without an ABI version marker should be refused") {
        config.mYAML["modmqttd"]["converter_plugins"][0] = "noabiconv.so";

        MockedModMqttServerThread server(config.toString(), false);
        server.start();
        server.stop();

        REQUIRE(server.initOk() == false);
        server.requireException<modmqttd::ConfigurationException>("does not declare converter_plugin_abi_version");
    }
}
