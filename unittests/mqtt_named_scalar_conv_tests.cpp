#include "catch2/catch_all.hpp"
#include "mockedserver.hpp"
#include "jsonutils.hpp"
#include "defaults.hpp"
#include "yaml_utils.hpp"


TEST_CASE ("Named scalar should output converted value") {

static const std::string config = R"(
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
        name: some_name
        register: tcptest.1.2
        register_type: input
        converter: std.divide(10,3)
)";


    MockedModMqttServerThread server(config);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 32456);
    server.start();
    // default mocked modbus read time is 5ms per register
    server.waitForPublish("test_sensor/state");
    REQUIRE_JSON(server.mqttValue("test_sensor/state"), "{ \"some_name\": 3245.6}");
    server.stop();
}


std::string create_config(const std::string& pConv) {
const std::string tmpl(R"(
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
        converter: CP
)");

    std::string config(tmpl);
    size_t index = config.find("CP");
    config.replace(index, pConv.size(), pConv.c_str());
    std::cout << config << std::endl;
    return config;
}

TEST_CASE ("Map converter") {
    SECTION("should publish mapped value") {
        //std::string config(tmpl).replace("CP", "std.map({24:\"twenty four\"})");
        std::string config = create_config("std.map('{24:\"twenty four\"}')");
        MockedModMqttServerThread server(config);
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 24);
        server.start();
        // default mocked modbus read time is 5ms per register
        server.waitForPublish("test_sensor/state");
        REQUIRE_JSON(server.mqttValue("test_sensor/state"), "twenty four");
        server.stop();
    }

    SECTION("with int values should be parsed") {
        std::string config = create_config("std.map('{24:1,55:-1}')");
        MockedModMqttServerThread server(config);
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 55);
        server.start();
        server.waitForPublish("test_sensor/state");
        REQUIRE_JSON(server.mqttValue("test_sensor/state"), "-1");
        server.stop();
    }

    SECTION("without braces should be parsed") {
        std::string config = create_config("std.map('24:1,55:-1')");
        MockedModMqttServerThread server(config);
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 24);
        server.start();
        server.waitForPublish("test_sensor/state");
        server.stop();
    }

    SECTION("without braces, with int values and spaces should be parsed") {
        std::string config = create_config("std.map('  24:1,   55:-1   ')");
        MockedModMqttServerThread server(config);
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 24);
        server.start();
        server.waitForPublish("test_sensor/state");
        server.stop();
    }

    SECTION("without braces, with string values and spaces should be parsed") {
        std::string config = create_config("std.map('  24:\"1\",   55:\"-1\"   ')");
        MockedModMqttServerThread server(config);
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 24);
        server.start();
        server.waitForPublish("test_sensor/state");
        server.stop();
    }

    SECTION("with converter spec on new line should be parsed") {
        //std::string config(tmpl).replace("CP", "std.map({24:\"twenty four\"})");
        std::string config = create_config(">\n          std.map('{24:  1}')");
        MockedModMqttServerThread server(config);
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 24);
        server.start();
        // default mocked modbus read time is 5ms per register
        server.waitForPublish("test_sensor/state");
        REQUIRE_JSON(server.mqttValue("test_sensor/state"), "1");
        server.stop();
    }

}
