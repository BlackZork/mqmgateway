#include <catch2/catch_all.hpp>
#include "libmodmqttconv/converterplugin.hpp"
#include "libmodmqttsrv/config.hpp"
#include "libmodmqttsrv/dll_import.hpp"

#include "testnumbers.hpp"
#include "plugin_utils.hpp"

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

#endif
