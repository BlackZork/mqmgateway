#include "catch2/catch_all.hpp"
#include "mockedserver.hpp"
#include "jsonutils.hpp"
#include "yaml_utils.hpp"
#include "testnumbers.hpp"

#ifdef HAVE_EXPRTK

/**
 * An expression result reaches createConvertedValue() only when the state is a
 * sequence: a single unnamed node with a converter is published as a bare
 * scalar by MqttPayload::generate() without ever building JSON.
 */
TEST_CASE("Expression value in a JSON list should be published as") {
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
  refresh: 1s
  broker:
    host: localhost
  objects:
    - topic: test_state
      state:
        - register: tcptest.1.2
          count: 2
          converter: expr.evaluate('uint32(R0, R1)', precision=0)
)");

    SECTION("a whole number when precision is zero") {
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, TestNumbers::Int::AB);
        server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, TestNumbers::Int::CD);
        server.start();

        server.waitForPublish("test_state/state");

        REQUIRE_JSON(server.mqttValue("test_state/state"),
                     ("[" + std::to_string(TestNumbers::Int::ABCD_as_uint32) + "]").c_str());
        server.stop();
    }

    SECTION("a truncated number when precision is zero and the result has a fraction") {
        config.mYAML["mqtt"]["objects"][0]["state"][0]["converter"] = "expr.evaluate('R0 / 3', precision=0)";

        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 10);
        server.start();

        server.waitForPublish("test_state/state");

        REQUIRE_JSON(server.mqttValue("test_state/state"), "[3]");
        server.stop();
    }

    SECTION("a rounded number when precision is set") {
        config.mYAML["mqtt"]["objects"][0]["state"][0]["converter"] = "expr.evaluate('R0 / 3', precision=3)";

        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 10);
        server.start();

        server.waitForPublish("test_state/state");

        REQUIRE_JSON(server.mqttValue("test_state/state"), "[3.333]");
        server.stop();
    }

    SECTION("six decimal places when no precision is set") {
        config.mYAML["mqtt"]["objects"][0]["state"][0]["converter"] = "expr.evaluate('R0 / 3')";

        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 10);
        server.start();

        server.waitForPublish("test_state/state");

        REQUIRE_JSON(server.mqttValue("test_state/state"), "[3.333333]");
        server.stop();
    }

    SECTION("exact digits when the whole result does not fit an int64") {
        // 2^32 times a uint32 needs more than 63 bits, so the payload has to be
        // written as an unsigned integer to survive
        config.mYAML["mqtt"]["objects"][0]["state"][0]["converter"] = "expr.evaluate('uint32(R0, R1) * 4294967296')";
        const uint64_t expected = static_cast<uint64_t>(TestNumbers::Int::ABCD_as_uint32) << 32;

        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, TestNumbers::Int::AB);
        server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, TestNumbers::Int::CD);
        server.start();

        server.waitForPublish("test_state/state");

        REQUIRE_JSON(server.mqttValue("test_state/state"),
                     ("[" + std::to_string(expected) + "]").c_str());
        server.stop();
    }
}

#endif
