#include <catch2/catch_all.hpp>
#include "libmodmqttconv/converterplugin.hpp"
#include "libmodmqttsrv/config.hpp"
#include "libmodmqttsrv/dll_import.hpp"

#include "jsonutils.hpp"
#include "mockedserver.hpp"
#include "testnumbers.hpp"
#include "plugin_utils.hpp"
#include "yaml_utils.hpp"

#ifdef HAVE_EXPRTK

namespace {

std::string
stdValue(PluginLoader& pLoader, const std::string& pName, bool pLowFirst, bool pSwapBytes, const ModbusRegisters& pInput) {
    std::shared_ptr<DataConverter> conv(pLoader.getConverter(pName));
    ConverterArgValues args(conv->getArgs());
    if (pLowFirst) {
        args.setArgValue(ConverterArg::sLowFirstArgName, "true");
    }
    if (pSwapBytes) {
        args.setArgValue(ConverterArg::sSwapBytesArgName, "true");
    }
    conv->setArgValues(args);
    return conv->toMqtt(pInput).getString();
}

std::string
exprValue(PluginLoader& pLoader, const std::string& pExpression, const ModbusRegisters& pInput) {
    std::shared_ptr<DataConverter> conv(pLoader.getConverter("evaluate"));
    ConverterArgValues args(conv->getArgs());
    args.setArgValue("expression", pExpression);
    conv->setArgValues(args);
    return conv->toMqtt(pInput).getString();
}

} // namespace

/**
 * The governing requirement of the 64-bit work: whatever a std converter
 * publishes for a set of registers, the matching expression publishes the same
 * text. std spells word and byte order as arguments, an expression spells word
 * order by the order it passes the registers and byte order with a bs suffix,
 * so the four combinations pair up as the sections below.
 */
TEST_CASE("std and expr should publish the same 64-bit value") {
    PluginLoader stdLoader("../stdconv/stdconv.so");
    PluginLoader exprLoader("../exprconv/exprconv.so");

    const ModbusRegisters intPlain({TestNumbers::Int64::AB, TestNumbers::Int64::CD, TestNumbers::Int64::EF, TestNumbers::Int64::GH});
    const ModbusRegisters intSwapped({TestNumbers::Int64::BA, TestNumbers::Int64::DC, TestNumbers::Int64::FE, TestNumbers::Int64::HG});
    const ModbusRegisters dblPlain(
        {TestNumbers::Double::AB, TestNumbers::Double::CD, TestNumbers::Double::EF, TestNumbers::Double::GH});
    const ModbusRegisters dblSwapped(
        {TestNumbers::Double::BA, TestNumbers::Double::DC, TestNumbers::Double::FE, TestNumbers::Double::HG});

#if LDBL_MANT_DIG >= 64
    SECTION("for uint64 in every word and byte order") {
        REQUIRE(stdValue(stdLoader, "uint64", false, false, intPlain) == exprValue(exprLoader, "uint64(R0, R1, R2, R3)", intPlain));
        REQUIRE(stdValue(stdLoader, "uint64", true, false, intPlain) == exprValue(exprLoader, "uint64(R3, R2, R1, R0)", intPlain));
        REQUIRE(stdValue(stdLoader, "uint64", false, true, intSwapped) == exprValue(exprLoader, "uint64bs(R0, R1, R2, R3)", intSwapped));
        REQUIRE(stdValue(stdLoader, "uint64", true, true, intSwapped) == exprValue(exprLoader, "uint64bs(R3, R2, R1, R0)", intSwapped));
    }

    // the value that fails if anything anywhere narrows through a double
    SECTION("for a uint64 above int64, digit for digit") {
        const std::string expected = std::to_string(TestNumbers::Int64::ABCDEFGH_as_uint64);

        REQUIRE(stdValue(stdLoader, "uint64", false, false, intPlain) == expected);
        REQUIRE(exprValue(exprLoader, "uint64(R0, R1, R2, R3)", intPlain) == expected);
    }

    SECTION("for int64 in every word and byte order") {
        REQUIRE(stdValue(stdLoader, "int64", false, false, intPlain) == exprValue(exprLoader, "int64(R0, R1, R2, R3)", intPlain));
        REQUIRE(stdValue(stdLoader, "int64", true, false, intPlain) == exprValue(exprLoader, "int64(R3, R2, R1, R0)", intPlain));
        REQUIRE(stdValue(stdLoader, "int64", false, true, intSwapped) == exprValue(exprLoader, "int64bs(R0, R1, R2, R3)", intSwapped));
        REQUIRE(stdValue(stdLoader, "int64", true, true, intSwapped) == exprValue(exprLoader, "int64bs(R3, R2, R1, R0)", intSwapped));
    }

#endif

    // std.float64 hands MqttValue a double and the expression a long double, so
    // this also pins down that the templated format() renders both the same.
    // Not gated on the mantissa width: a double survives a long double of any
    // width, so this section runs on 32-bit arm too.
    SECTION("for float64 in every word and byte order") {
        REQUIRE(stdValue(stdLoader, "float64", false, false, dblPlain) == exprValue(exprLoader, "flt64(R0, R1, R2, R3)", dblPlain));
        REQUIRE(stdValue(stdLoader, "float64", true, false, dblPlain) == exprValue(exprLoader, "flt64(R3, R2, R1, R0)", dblPlain));
        REQUIRE(stdValue(stdLoader, "float64", false, true, dblSwapped) == exprValue(exprLoader, "flt64bs(R0, R1, R2, R3)", dblSwapped));
        REQUIRE(stdValue(stdLoader, "float64", true, true, dblSwapped) == exprValue(exprLoader, "flt64bs(R3, R2, R1, R0)", dblSwapped));
    }
}

#if LDBL_MANT_DIG >= 64

/**
 * The same requirement again, but through the daemon rather than the plugins on
 * their own, so the publish path is included: a std converter reaches
 * createConvertedValue() as UINT64 and an expression as FLOAT64, and those are
 * different arms that have to agree.
 *
 * This is the only config in the suite that loads both plugins at once.
 */
TEST_CASE("A polled 64-bit register should publish the same value through std and expr") {
    TestConfig config(R"(
modmqttd:
  converter_search_path:
    - build/stdconv
    - build/exprconv
  converter_plugins:
    - stdconv.so
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
    - topic: std_state
      state:
        register: tcptest.1.2
        count: 4
        converter: std.uint64()
    - topic: expr_state
      state:
        register: tcptest.1.2
        count: 4
        converter: expr.evaluate('uint64(R0, R1, R2, R3)')
    - topic: both_state
      state:
        - register: tcptest.1.2
          count: 4
          converter: std.uint64()
        - register: tcptest.1.2
          count: 4
          converter: expr.evaluate('uint64(R0, R1, R2, R3)')
)");

    const std::string expected = std::to_string(TestNumbers::Int64::ABCDEFGH_as_uint64);

    SECTION("on its own state topic") {
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, TestNumbers::Int64::AB);
        server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, TestNumbers::Int64::CD);
        server.setModbusRegisterValue("tcptest", 1, 4, modmqttd::RegisterType::HOLDING, TestNumbers::Int64::EF);
        server.setModbusRegisterValue("tcptest", 1, 5, modmqttd::RegisterType::HOLDING, TestNumbers::Int64::GH);
        server.start();

        server.waitForPublish("std_state/state");
        server.waitForPublish("expr_state/state");

        REQUIRE(server.mqttValue("std_state/state") == expected);
        REQUIRE(server.mqttValue("expr_state/state") == server.mqttValue("std_state/state"));
        server.stop();
    }

    // a list topic is what routes both values through createConvertedValue(),
    // where the digits are lost if the FLOAT64 arm falls back to writer.Double()
    SECTION("side by side in a JSON list") {
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, TestNumbers::Int64::AB);
        server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, TestNumbers::Int64::CD);
        server.setModbusRegisterValue("tcptest", 1, 4, modmqttd::RegisterType::HOLDING, TestNumbers::Int64::EF);
        server.setModbusRegisterValue("tcptest", 1, 5, modmqttd::RegisterType::HOLDING, TestNumbers::Int64::GH);
        server.start();

        server.waitForPublish("both_state/state");

        REQUIRE_JSON(server.mqttValue("both_state/state"), ("[" + expected + "," + expected + "]").c_str());
        server.stop();
    }
}

/**
 * The write direction, where the payload arrives as text and the exactness is
 * decided by which accessor parses it.
 */
TEST_CASE("A 64-bit command should write the same registers through std and expr") {
    TestConfig config(R"(
modmqttd:
  converter_search_path:
    - build/stdconv
    - build/exprconv
  converter_plugins:
    - stdconv.so
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
    - topic: std_cmd
      commands:
        - name: set
          register: tcptest.1.10
          register_type: holding
          count: 4
          converter: std.uint64()
    - topic: expr_cmd
      commands:
        - name: set
          register: tcptest.1.20
          register_type: holding
          count: 4
          converter: expr.evaluate('M0', write_as='uint64')
)");

    MockedModMqttServerThread server(config.toString());
    server.start();

    server.waitForSubscription("std_cmd/set");
    server.waitForSubscription("expr_cmd/set");

    const std::string payload = std::to_string(TestNumbers::Int64::ABCDEFGH_as_uint64);
    server.publish("std_cmd/set", payload);
    server.publish("expr_cmd/set", payload);

    const uint16_t words[] = {TestNumbers::Int64::AB, TestNumbers::Int64::CD, TestNumbers::Int64::EF, TestNumbers::Int64::GH};
    for (int i = 0; i < 4; i++) {
        server.waitForModbusValue("tcptest", 1, 10 + i, modmqttd::RegisterType::HOLDING, words[i]);
        server.waitForModbusValue("tcptest", 1, 20 + i, modmqttd::RegisterType::HOLDING, words[i]);
    }

    server.stop();
}

#endif

#endif
