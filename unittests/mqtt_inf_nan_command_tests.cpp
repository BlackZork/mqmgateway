#include "catch2/catch_all.hpp"
#include "log_capture.hpp"
#include "mockedserver.hpp"
#include "testnumbers.hpp"
#include "yaml_utils.hpp"

TEST_CASE("A float32 command should write") {

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
  broker:
    host: localhost
  objects:
    - topic: setpoint
      commands:
        - name: set
          register: tcptest.1.2
          register_type: holding
          count: 2
          converter: std.float32()
)");

    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 1);
    server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, 1);
    server.start();
    server.waitForSubscription("setpoint/set");

    SECTION("a bare NaN as not-a-number bits") {
        server.publish("setpoint/set", "NaN");

        server.waitForModbusValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, TestNumbers::Float::NAN_HIGH);
        server.waitForModbusValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, TestNumbers::Float::NAN_LOW);
    }

    SECTION("a quoted -Infinity as negative infinity bits") {
        server.publish("setpoint/set", "\"-Infinity\"");

        server.waitForModbusValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, TestNumbers::Float::NEG_INF_HIGH);
        server.waitForModbusValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, TestNumbers::Float::NEG_INF_LOW);
    }

    server.stop();
}

/**
 * The mocked broker hands a published command to the daemon synchronously, so
 * a rejected payload has already been logged when publish() returns. A later
 * write to the sentinel register on the same slave then proves that nothing was
 * queued for the rejected one.
 */
TEST_CASE("A null command payload should") {

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
  broker:
    host: localhost
  objects:
    - topic: setpoint
      commands:
        - name: set
          register: tcptest.1.2
          register_type: holding
          count: 2
          converter: std.float32()
        - name: set_nan
          register: tcptest.1.4
          register_type: holding
          count: 2
          converter: std.float32()
          null_as_nan: true
        - name: set_int
          register: tcptest.1.6
          register_type: holding
          converter: std.int16()
          null_as_nan: true
        - name: sentinel
          register: tcptest.1.10
          register_type: holding
)");

    // created before the server and destroyed after it: the logger's sink list
    // must not change while the server threads are logging
    LogCapture logs;
    MockedModMqttServerThread server(config.toString());
    for (int reg = 2; reg <= 6; reg++) {
        server.setModbusRegisterValue("tcptest", 1, reg, modmqttd::RegisterType::HOLDING, 1);
    }
    server.setModbusRegisterValue("tcptest", 1, 10, modmqttd::RegisterType::HOLDING, 0);
    server.start();
    server.waitForSubscription("setpoint/set");
    server.waitForSubscription("setpoint/set_nan");
    server.waitForSubscription("setpoint/set_int");
    server.waitForSubscription("setpoint/sentinel");

    SECTION("be rejected without null_as_nan") {
        server.publish("setpoint/set", "null");
        REQUIRE(logs.contains("Converter error for setpoint/set"));

        server.publish("setpoint/sentinel", "7");
        server.waitForModbusValue("tcptest", 1, 10, modmqttd::RegisterType::HOLDING, 7);

        REQUIRE(server.getModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING) == 1);
        REQUIRE(server.getModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING) == 1);
    }

    SECTION("be written as not-a-number bits with null_as_nan") {
        server.publish("setpoint/set_nan", "null");

        server.waitForModbusValue("tcptest", 1, 4, modmqttd::RegisterType::HOLDING, TestNumbers::Float::NAN_HIGH);
        server.waitForModbusValue("tcptest", 1, 5, modmqttd::RegisterType::HOLDING, TestNumbers::Float::NAN_LOW);
    }

    SECTION("be written as not-a-number bits with null_as_nan after a line end or a terminator") {
        server.publish("setpoint/set_nan", "null\r\n");
        server.waitForModbusValue("tcptest", 1, 4, modmqttd::RegisterType::HOLDING, TestNumbers::Float::NAN_HIGH);

        server.publish("setpoint/set_nan", "0");
        server.waitForModbusValue("tcptest", 1, 4, modmqttd::RegisterType::HOLDING, 0);

        server.publish("setpoint/set_nan", std::string("null\0", 5));
        server.waitForModbusValue("tcptest", 1, 4, modmqttd::RegisterType::HOLDING, TestNumbers::Float::NAN_HIGH);
    }

    SECTION("be rejected by an integer converter even with null_as_nan") {
        server.publish("setpoint/set_int", "null");
        REQUIRE(logs.contains("Converter error for setpoint/set_int"));

        server.publish("setpoint/sentinel", "7");
        server.waitForModbusValue("tcptest", 1, 10, modmqttd::RegisterType::HOLDING, 7);

        REQUIRE(server.getModbusRegisterValue("tcptest", 1, 6, modmqttd::RegisterType::HOLDING) == 1);
    }

    server.stop();
}
