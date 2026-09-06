#include <catch2/catch_all.hpp>
#include "libmodmqttconv/converterplugin.hpp"
#include "libmodmqttsrv/config.hpp"
#include "libmodmqttsrv/dll_import.hpp"

#include "testnumbers.hpp"
#include "plugin_utils.hpp"

#ifdef HAVE_EXPRTK

namespace {

ModbusRegisters
writeWith(PluginLoader& pLoader, const std::string& pWriteAs, bool pLowFirst, const MqttValue& pInput, int pRegisterCount) {
    std::shared_ptr<DataConverter> conv(pLoader.getConverter("evaluate"));
    ConverterArgValues args(conv->getArgs());
    args.setArgValue("expression", "M0");
    args.setArgValue("write_as", pWriteAs);
    if (pLowFirst) {
        args.setArgValue(ConverterArg::sLowFirstArgName, "true");
    }
    conv->setArgValues(args);
    return conv->toModbus(pInput, pRegisterCount);
}

} // namespace

#if LDBL_MANT_DIG >= 64

TEST_CASE("exprtk should write uint64") {
    PluginLoader loader("../exprconv/exprconv.so");

    // above INT64_MAX, so nothing on the way to the registers may go through a
    // signed or a 53-bit carrier
    const MqttValue input = MqttValue::fromUInt64(TestNumbers::Int64::ABCDEFGH_as_uint64);

    SECTION("to four registers, high word first") {
        const ModbusRegisters expected(
            {TestNumbers::Int64::AB, TestNumbers::Int64::CD, TestNumbers::Int64::EF, TestNumbers::Int64::GH});

        REQUIRE(writeWith(loader, "uint64", false, input, 4).values() == expected.values());
    }

    SECTION("to four registers, low word first") {
        const ModbusRegisters expected(
            {TestNumbers::Int64::GH, TestNumbers::Int64::EF, TestNumbers::Int64::CD, TestNumbers::Int64::AB});

        REQUIRE(writeWith(loader, "uint64", true, input, 4).values() == expected.values());
    }

    SECTION("to four registers byte swapped, high word first") {
        const ModbusRegisters expected(
            {TestNumbers::Int64::BA, TestNumbers::Int64::DC, TestNumbers::Int64::FE, TestNumbers::Int64::HG});

        REQUIRE(writeWith(loader, "uint64bs", false, input, 4).values() == expected.values());
    }

    SECTION("to four registers byte swapped, low word first") {
        const ModbusRegisters expected(
            {TestNumbers::Int64::HG, TestNumbers::Int64::FE, TestNumbers::Int64::DC, TestNumbers::Int64::BA});

        REQUIRE(writeWith(loader, "uint64bs", true, input, 4).values() == expected.values());
    }

    // a command topic delivers its payload as text, so this is the shape a real
    // write arrives in and the one that would round if it were parsed as double
    SECTION("from a decimal string payload") {
        const MqttValue text = MqttValue::fromString(std::to_string(TestNumbers::Int64::ABCDEFGH_as_uint64));
        const ModbusRegisters expected(
            {TestNumbers::Int64::AB, TestNumbers::Int64::CD, TestNumbers::Int64::EF, TestNumbers::Int64::GH});

        REQUIRE(writeWith(loader, "uint64", false, text, 4).values() == expected.values());
    }

    SECTION("and refuse a register count that is not four") {
        REQUIRE_THROWS_AS(writeWith(loader, "uint64", false, input, 2), ConvException);
    }

    SECTION("and take four registers per returned value") {
        std::shared_ptr<DataConverter> conv(loader.getConverter("evaluate"));
        ConverterArgValues args(conv->getArgs());
        args.setArgValue("expression", "return [M0, M0]");
        args.setArgValue("write_as", "uint64");

        conv->setArgValues(args);
        const ModbusRegisters converted = conv->toModbus(input, 8);

        REQUIRE(converted.getCount() == 8);
        REQUIRE(converted.getValue(0) == TestNumbers::Int64::AB);
        REQUIRE(converted.getValue(4) == TestNumbers::Int64::AB);
    }
}

TEST_CASE("exprtk should write int64") {
    PluginLoader loader("../exprconv/exprconv.so");

    // the test pattern's top bit is set, so this value is negative
    const MqttValue input = MqttValue::fromInt64(TestNumbers::Int64::ABCDEFGH_as_int64);

    SECTION("to four registers, high word first") {
        const ModbusRegisters expected(
            {TestNumbers::Int64::AB, TestNumbers::Int64::CD, TestNumbers::Int64::EF, TestNumbers::Int64::GH});

        REQUIRE(writeWith(loader, "int64", false, input, 4).values() == expected.values());
    }

    SECTION("to four registers, low word first") {
        const ModbusRegisters expected(
            {TestNumbers::Int64::GH, TestNumbers::Int64::EF, TestNumbers::Int64::CD, TestNumbers::Int64::AB});

        REQUIRE(writeWith(loader, "int64", true, input, 4).values() == expected.values());
    }

    SECTION("to four registers byte swapped, high word first") {
        const ModbusRegisters expected(
            {TestNumbers::Int64::BA, TestNumbers::Int64::DC, TestNumbers::Int64::FE, TestNumbers::Int64::HG});

        REQUIRE(writeWith(loader, "int64bs", false, input, 4).values() == expected.values());
    }

    SECTION("and refuse a value above int64") {
        const MqttValue tooBig = MqttValue::fromUInt64(TestNumbers::Int64::ABCDEFGH_as_uint64);

        REQUIRE_THROWS_AS(writeWith(loader, "int64", false, tooBig, 4), ConvException);
    }
}

#endif

// flt64 writes an IEEE-754 double, which a long double holds exactly at any
// width, so these run on 32-bit arm too
TEST_CASE("exprtk should write float64") {
    PluginLoader loader("../exprconv/exprconv.so");

    const MqttValue input = MqttValue::fromDouble(TestNumbers::Double::ABCDEFGH_as_double);

    SECTION("to four registers, high word first") {
        const ModbusRegisters expected(
            {TestNumbers::Double::AB, TestNumbers::Double::CD, TestNumbers::Double::EF, TestNumbers::Double::GH});

        REQUIRE(writeWith(loader, "flt64", false, input, 4).values() == expected.values());
    }

    SECTION("to four registers, low word first") {
        const ModbusRegisters expected(
            {TestNumbers::Double::GH, TestNumbers::Double::EF, TestNumbers::Double::CD, TestNumbers::Double::AB});

        REQUIRE(writeWith(loader, "flt64", true, input, 4).values() == expected.values());
    }

    SECTION("to four registers byte swapped, high word first") {
        const ModbusRegisters expected(
            {TestNumbers::Double::BA, TestNumbers::Double::DC, TestNumbers::Double::FE, TestNumbers::Double::HG});

        REQUIRE(writeWith(loader, "flt64bs", false, input, 4).values() == expected.values());
    }

    SECTION("and refuse a register count that is not four") {
        REQUIRE_THROWS_AS(writeWith(loader, "flt64", false, input, 2), ConvException);
    }
}

#endif
