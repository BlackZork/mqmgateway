#include "catch2/catch_all.hpp"

#include "log_capture.hpp"
#include "mockedserver.hpp"
#include "testnumbers.hpp"
#include "yaml_utils.hpp"

TEST_CASE("A converter used with an unexpected register count") {
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
        count: 2
        converter: std.int32()
)");

    SECTION("should be reported at startup and keep the object working") {
        config.mYAML["mqtt"]["objects"][0]["state"]["count"] = 1;

        LogCapture logs;
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 5);
        server.start();
        server.waitForPublish("test_sensor/state");

        REQUIRE(server.mqttValue("test_sensor/state") == "5");
        server.stop();

        REQUIRE(logs.contains("converter std.int32() is designed for 2 register(s), but this register has 1"));
    }

    SECTION("should not be reported when the count matches") {
        LogCapture logs;
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, TestNumbers::Int::AB);
        server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::INPUT, TestNumbers::Int::CD);
        server.start();
        server.waitForPublish("test_sensor/state");
        server.stop();

        REQUIRE_FALSE(logs.contains("is designed for"));
    }

    SECTION("should not be reported for a converter that accepts any count") {
        config.mYAML["mqtt"]["objects"][0]["state"]["count"] = 1;
        config.mYAML["mqtt"]["objects"][0]["state"]["converter"] = "std.divide(2)";

        LogCapture logs;
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 10);
        server.start();
        server.waitForPublish("test_sensor/state");
        server.stop();

        REQUIRE_FALSE(logs.contains("is designed for"));
    }

    SECTION("should be reported for a command register count") {
        config.mYAML["mqtt"]["objects"][0]["commands"][0]["name"] = "set_value";
        config.mYAML["mqtt"]["objects"][0]["commands"][0]["register"] = "tcptest.1.20";
        config.mYAML["mqtt"]["objects"][0]["commands"][0]["register_type"] = "holding";
        config.mYAML["mqtt"]["objects"][0]["commands"][0]["converter"] = "std.float32()";

        LogCapture logs;
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, TestNumbers::Int::AB);
        server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::INPUT, TestNumbers::Int::CD);
        server.start();
        server.waitForPublish("test_sensor/state");
        server.stop();

        REQUIRE(logs.contains("converter std.float32() is designed for 2 register(s), but command test_sensor/set_value has 1"));
    }
}
