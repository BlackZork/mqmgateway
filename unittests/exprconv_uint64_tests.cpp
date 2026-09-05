#include <catch2/catch_all.hpp>
#include "libmodmqttconv/converterplugin.hpp"
#include "libmodmqttsrv/config.hpp"
#include "libmodmqttsrv/dll_import.hpp"

#include "testnumbers.hpp"
#include "plugin_utils.hpp"

#ifdef HAVE_EXPRTK
#if LDBL_MANT_DIG >= 64

TEST_CASE("exprtk should read uint64") {
    PluginLoader loader("../exprconv/exprconv.so");

    std::shared_ptr<DataConverter> conv(loader.getConverter("evaluate"));
    ConverterArgValues args(conv->getArgs());

    SECTION("from four registers") {
        args.setArgValue("expression", "uint64(R0, R1, R2, R3)");
        const ModbusRegisters input({TestNumbers::Int64::AB, TestNumbers::Int64::CD, TestNumbers::Int64::EF, TestNumbers::Int64::GH});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getUInt64() == TestNumbers::Int64::ABCDEFGH_as_uint64);
    }

    SECTION("from four registers byte swapped") {
        args.setArgValue("expression", "uint64bs(R0, R1, R2, R3)");
        const ModbusRegisters input({TestNumbers::Int64::BA, TestNumbers::Int64::DC, TestNumbers::Int64::FE, TestNumbers::Int64::HG});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getUInt64() == TestNumbers::Int64::ABCDEFGH_as_uint64);
    }

    SECTION("from four registers in reverse word order") {
        args.setArgValue("expression", "uint64(R3, R2, R1, R0)");
        const ModbusRegisters input({TestNumbers::Int64::GH, TestNumbers::Int64::EF, TestNumbers::Int64::CD, TestNumbers::Int64::AB});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getUInt64() == TestNumbers::Int64::ABCDEFGH_as_uint64);
    }

    SECTION("from four registers in reverse word order byte swapped") {
        args.setArgValue("expression", "uint64bs(R3, R2, R1, R0)");
        const ModbusRegisters input({TestNumbers::Int64::HG, TestNumbers::Int64::FE, TestNumbers::Int64::DC, TestNumbers::Int64::BA});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getUInt64() == TestNumbers::Int64::ABCDEFGH_as_uint64);
    }

    // the point of the whole milestone: the digits survive publication, not
    // just the helper, so the value is above what a double could have carried
    SECTION("and publish every digit of a value above int64") {
        args.setArgValue("expression", "uint64(R0, R1, R2, R3)");
        const ModbusRegisters input({TestNumbers::Int64::AB, TestNumbers::Int64::CD, TestNumbers::Int64::EF, TestNumbers::Int64::GH});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getString() == std::to_string(TestNumbers::Int64::ABCDEFGH_as_uint64));
        REQUIRE(output.getSourceType() == MqttValue::SourceType::FLOAT64);
    }

    // arithmetic on the result proves the engine is exact, not only the helper
    SECTION("and stay exact when the result is used in arithmetic") {
        args.setArgValue("expression", "uint64(R0, R1, R2, R3) - 1");
        const ModbusRegisters input({TestNumbers::Int64::AB, TestNumbers::Int64::CD, TestNumbers::Int64::EF, TestNumbers::Int64::GH});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getUInt64() == TestNumbers::Int64::ABCDEFGH_as_uint64 - 1);
    }
}

#endif
#endif
