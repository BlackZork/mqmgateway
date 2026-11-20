#include "catch2/catch_all.hpp"
#include "mockedserver.hpp"
#include "testnumbers.hpp"
#include "yaml_utils.hpp"

// 1.5 as a float32, high word first
static const uint16_t sOneAndHalfHigh = 0x3FC0;
static const uint16_t sOneAndHalfLow = 0x0000;

TEST_CASE("A NaN reading should be published") {
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
    - topic: plain
      state:
        register: tcptest.1.2
        count: 2
        converter: std.float32()
    - topic: list
      state:
        - register: tcptest.1.2
          count: 2
          converter: std.float32()
        - register: tcptest.1.4
          count: 2
          converter: std.float32()
    - topic: map
      state:
        - name: power
          register: tcptest.1.2
          count: 2
          converter: std.float32()
        - name: energy
          register: tcptest.1.4
          count: 2
          converter: std.float32()
)");

    SECTION("as null by default") {
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, TestNumbers::Float::NAN_HIGH);
        server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, TestNumbers::Float::NAN_LOW);
        server.setModbusRegisterValue("tcptest", 1, 4, modmqttd::RegisterType::HOLDING, sOneAndHalfHigh);
        server.setModbusRegisterValue("tcptest", 1, 5, modmqttd::RegisterType::HOLDING, sOneAndHalfLow);
        server.start();
        server.waitForPublish("plain/state");
        server.waitForPublish("list/state");
        server.waitForPublish("map/state");

        REQUIRE(server.mqttValue("plain/state") == "null");
        REQUIRE(server.mqttValue("list/state") == "[null,1.5]");
        REQUIRE(server.mqttValue("map/state") == R"({"power":null,"energy":1.5})");
        server.stop();
    }

    SECTION("as a bare NaN when publish_inf_nan is literal") {
        config.mYAML["mqtt"]["publish_inf_nan"] = "literal";
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, TestNumbers::Float::NAN_HIGH);
        server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, TestNumbers::Float::NAN_LOW);
        server.setModbusRegisterValue("tcptest", 1, 4, modmqttd::RegisterType::HOLDING, sOneAndHalfHigh);
        server.setModbusRegisterValue("tcptest", 1, 5, modmqttd::RegisterType::HOLDING, sOneAndHalfLow);
        server.start();
        server.waitForPublish("plain/state");
        server.waitForPublish("list/state");
        server.waitForPublish("map/state");

        REQUIRE(server.mqttValue("plain/state") == "NaN");
        REQUIRE(server.mqttValue("list/state") == "[NaN,1.5]");
        REQUIRE(server.mqttValue("map/state") == R"({"power":NaN,"energy":1.5})");
        server.stop();
    }

    SECTION("as a quoted NaN when publish_inf_nan is string") {
        config.mYAML["mqtt"]["publish_inf_nan"] = "string";
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, TestNumbers::Float::NAN_HIGH);
        server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, TestNumbers::Float::NAN_LOW);
        server.setModbusRegisterValue("tcptest", 1, 4, modmqttd::RegisterType::HOLDING, sOneAndHalfHigh);
        server.setModbusRegisterValue("tcptest", 1, 5, modmqttd::RegisterType::HOLDING, sOneAndHalfLow);
        server.start();
        server.waitForPublish("plain/state");
        server.waitForPublish("list/state");
        server.waitForPublish("map/state");

        REQUIRE(server.mqttValue("plain/state") == R"("NaN")");
        REQUIRE(server.mqttValue("list/state") == R"(["NaN",1.5])");
        REQUIRE(server.mqttValue("map/state") == R"({"power":"NaN","energy":1.5})");
        server.stop();
    }

    SECTION("in the mode of its own topic over the global one") {
        config.mYAML["mqtt"]["publish_inf_nan"] = "literal";
        config.mYAML["mqtt"]["objects"][0]["publish_inf_nan"] = "string";
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, TestNumbers::Float::NAN_HIGH);
        server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, TestNumbers::Float::NAN_LOW);
        server.setModbusRegisterValue("tcptest", 1, 4, modmqttd::RegisterType::HOLDING, sOneAndHalfHigh);
        server.setModbusRegisterValue("tcptest", 1, 5, modmqttd::RegisterType::HOLDING, sOneAndHalfLow);
        server.start();
        server.waitForPublish("plain/state");
        server.waitForPublish("list/state");

        REQUIRE(server.mqttValue("plain/state") == R"("NaN")");
        REQUIRE(server.mqttValue("list/state") == "[NaN,1.5]");
        server.stop();
    }

    SECTION("as null when the converter drops the decimals") {
        config.mYAML["mqtt"]["objects"][1]["state"][0]["converter"] = "std.float32(precision=0)";
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, TestNumbers::Float::NAN_HIGH);
        server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, TestNumbers::Float::NAN_LOW);
        server.setModbusRegisterValue("tcptest", 1, 4, modmqttd::RegisterType::HOLDING, sOneAndHalfHigh);
        server.setModbusRegisterValue("tcptest", 1, 5, modmqttd::RegisterType::HOLDING, sOneAndHalfLow);
        server.start();
        server.waitForPublish("list/state");

        REQUIRE(server.mqttValue("list/state") == "[null,1.5]");
        server.stop();
    }
}

TEST_CASE("An infinite reading should be published") {
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
    - topic: list
      state:
        - register: tcptest.1.2
          count: 2
          converter: std.float32()
        - register: tcptest.1.4
          count: 2
          converter: std.float32()
)");

    SECTION("as null by default") {
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, TestNumbers::Float::POS_INF_HIGH);
        server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, TestNumbers::Float::POS_INF_LOW);
        server.setModbusRegisterValue("tcptest", 1, 4, modmqttd::RegisterType::HOLDING, TestNumbers::Float::NEG_INF_HIGH);
        server.setModbusRegisterValue("tcptest", 1, 5, modmqttd::RegisterType::HOLDING, TestNumbers::Float::NEG_INF_LOW);
        server.start();
        server.waitForPublish("list/state");

        REQUIRE(server.mqttValue("list/state") == "[null,null]");
        server.stop();
    }

    SECTION("as bare signed tokens when publish_inf_nan is literal") {
        config.mYAML["mqtt"]["publish_inf_nan"] = "literal";
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, TestNumbers::Float::POS_INF_HIGH);
        server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, TestNumbers::Float::POS_INF_LOW);
        server.setModbusRegisterValue("tcptest", 1, 4, modmqttd::RegisterType::HOLDING, TestNumbers::Float::NEG_INF_HIGH);
        server.setModbusRegisterValue("tcptest", 1, 5, modmqttd::RegisterType::HOLDING, TestNumbers::Float::NEG_INF_LOW);
        server.start();
        server.waitForPublish("list/state");

        REQUIRE(server.mqttValue("list/state") == "[Infinity,-Infinity]");
        server.stop();
    }

    SECTION("as quoted signed tokens when publish_inf_nan is string") {
        config.mYAML["mqtt"]["publish_inf_nan"] = "string";
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, TestNumbers::Float::POS_INF_HIGH);
        server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, TestNumbers::Float::POS_INF_LOW);
        server.setModbusRegisterValue("tcptest", 1, 4, modmqttd::RegisterType::HOLDING, TestNumbers::Float::NEG_INF_HIGH);
        server.setModbusRegisterValue("tcptest", 1, 5, modmqttd::RegisterType::HOLDING, TestNumbers::Float::NEG_INF_LOW);
        server.start();
        server.waitForPublish("list/state");

        REQUIRE(server.mqttValue("list/state") == R"(["Infinity","-Infinity"])");
        server.stop();
    }
}

TEST_CASE("A float32 reading published without decimals should") {
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
    - topic: list
      state:
        - register: tcptest.1.2
          count: 2
          converter: std.float32(precision=0)
)");

    SECTION("keep every digit of a whole value beyond int64") {
        MockedModMqttServerThread server(config.toString());
        // 1e19 as a float32 is 9999999980506447872, above INT64_MAX
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 0x5F0A);
        server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, 0xC723);
        server.start();
        server.waitForPublish("list/state");

        REQUIRE(server.mqttValue("list/state") == "[9999999980506447872]");
        server.stop();
    }
}

TEST_CASE("publish_inf_nan should") {
    SECTION("read an unquoted null as null") {
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
  publish_inf_nan: literal
  broker:
    host: localhost
  objects:
    - topic: plain
      publish_inf_nan: null
      state:
        register: tcptest.1.2
        count: 2
        converter: std.float32()
)");
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, TestNumbers::Float::NAN_HIGH);
        server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, TestNumbers::Float::NAN_LOW);
        server.start();
        server.waitForPublish("plain/state");

        REQUIRE(server.mqttValue("plain/state") == "null");
        server.stop();
    }

    SECTION("reject an unknown value with its line number") {
        // raw text rather than TestConfig, which re-emits the YAML and so moves its lines
        const std::string config = R"(modmqttd:
modbus:
  networks:
    - name: tcptest
      address: localhost
      port: 501
mqtt:
  client_id: mqtt_test
  publish_inf_nan: nan
  broker:
    host: localhost
  objects:
    - topic: plain
      state:
        register: tcptest.1.2
)";
        MockedModMqttServerThread server(config, false);
        server.start();
        server.stop();

        server.requireException<modmqttd::ConfigurationException>("Invalid publish_inf_nan 'nan', valid values are: null, literal, string");
        const modmqttd::ConfigurationException& err(dynamic_cast<const modmqttd::ConfigurationException&>(server.getException()));
        REQUIRE(err.mLineNumber == 9);
    }
}
