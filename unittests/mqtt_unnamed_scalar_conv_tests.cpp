#include <catch2/catch_all.hpp>
#include "mockedserver.hpp"
#include "yaml_utils.hpp"
#include "jsonutils.hpp"


TEST_CASE ("Unnamed scalar should output converted value") {

TestConfig config(R"(
modmqttd:
  converter_search_path:
    - build/stdconv
  converter_plugins:
    - stdconv.so
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
    - topic: test_sensor
      state:
        register: tcptest.1.2
        register_type: input
        converter: std.divide(1000,precision=3)
)");


    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 32456);
    server.start();
    // default mocked modbus read time is 5ms per register
    server.waitForPublish("test_sensor/state");
    REQUIRE(server.mqttValue("test_sensor/state") == "32.456");
    server.stop();
}


TEST_CASE ("Uint32 value should be published") {

TestConfig config(R"(
modmqttd:
  converter_search_path:
    - build/stdconv
  converter_plugins:
    - stdconv.so
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
    - topic: test_sensor
      state:
        register: tcptest.1.2
        count: 2
        register_type: input
        converter: std.uint32()
)");


    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 0x9234);
    server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::INPUT, 0xABCD);
    server.start();
    // default mocked modbus read time is 5ms per register
    server.waitForPublish("test_sensor/state");
    REQUIRE(server.mqttValue("test_sensor/state") == "2452925389");
    server.stop();
}


TEST_CASE ("Map converter") {

    TestConfig config(R"(
modmqttd:
  converter_search_path:
    - build/stdconv
  converter_plugins:
    - stdconv.so
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
    - topic: test_sensor
      state:
        register: tcptest.1.2
        register_type: input
)");

    SECTION("should publish mapped value") {
        config.mYAML["mqtt"]["objects"][0]["state"]["converter"] = "std.map('{24:\"twenty four\"}')";

        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 24);
        server.start();
        // default mocked modbus read time is 5ms per register
        server.waitForPublish("test_sensor/state");
        REQUIRE_JSON(server.mqttValue("test_sensor/state"), "twenty four");
        server.stop();
    }

    SECTION("with int values should be parsed") {
        config.mYAML["mqtt"]["objects"][0]["state"]["converter"] = "std.map('{24:1,55:-1}')";
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 55);
        server.start();
        server.waitForPublish("test_sensor/state");
        REQUIRE_JSON(server.mqttValue("test_sensor/state"), "-1");
        server.stop();
    }

    SECTION("without braces should be parsed") {
        config.mYAML["mqtt"]["objects"][0]["state"]["converter"] = "std.map('24:1,55:-1')";
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 24);
        server.start();
        server.waitForPublish("test_sensor/state");
        server.stop();
    }

    SECTION("without braces, with int values and spaces should be parsed") {
        config.mYAML["mqtt"]["objects"][0]["state"]["converter"] = "std.map('  24:1,   55:-1   ')";
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 24);
        server.start();
        server.waitForPublish("test_sensor/state");
        server.stop();
    }

    SECTION("without braces, with string values and spaces should be parsed") {
        config.mYAML["mqtt"]["objects"][0]["state"]["converter"] = "std.map('  24:\"1\",   55:\"-1\"   ')";
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 24);
        server.start();
        server.waitForPublish("test_sensor/state");
        server.stop();
    }
}

TEST_CASE ("Map converter with spec on new line should be parsed") {

TestConfig config(R"(
modmqttd:
  converter_search_path:
    - build/stdconv
  converter_plugins:
    - stdconv.so
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
    - topic: test_sensor
      state:
        register: tcptest.1.2
        register_type: input
        converter: >
            std.map('{24:  1}')
)");
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 24);
        server.start();
        // default mocked modbus read time is 5ms per register
        server.waitForPublish("test_sensor/state");
        REQUIRE_JSON(server.mqttValue("test_sensor/state"), "1");
        server.stop();
}


TEST_CASE ("Two topics for single register value") {

TestConfig config(R"(
modmqttd:
  converter_search_path:
    - build/stdconv
  converter_plugins:
    - stdconv.so
modbus:
  networks:
    - name: tcptest
      address: localhost
      port: 501
mqtt:
  client_id: mqtt_test
  refresh: 30ms
  broker:
    host: localhost
  objects:
    - topic: bit_1
      state:
        register: tcptest.1.2
        converter: std.bit(1)
    - topic: bit_2
      state:
        register: tcptest.1.2
        converter: std.bit(2)
)");

    SECTION("should publish only if value after conversion is changed") {
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 0x0);
        server.start();

        server.waitForPublish("bit_1/state");
        REQUIRE(server.mqttValue("bit_1/state") == "0");

        server.waitForPublish("bit_2/state");
        REQUIRE(server.mqttValue("bit_2/state") == "0");

        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 0x2);

        server.waitForPublish("bit_2/state");
        REQUIRE(server.mqttValue("bit_2/state") == "1");

        REQUIRE(server.mMqtt->getPublishCount("bit_1/state") == 1);
        REQUIRE(server.mMqtt->getPublishCount("bit_2/state") == 2);
        server.stop();
    }
}



