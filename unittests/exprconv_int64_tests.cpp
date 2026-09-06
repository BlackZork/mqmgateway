#include <catch2/catch_all.hpp>
#include "libmodmqttconv/converterplugin.hpp"
#include "libmodmqttsrv/config.hpp"
#include "libmodmqttsrv/dll_import.hpp"

#include "testnumbers.hpp"
#include "plugin_utils.hpp"

#ifdef HAVE_EXPRTK
#if LDBL_MANT_DIG >= 64

TEST_CASE("exprtk should read int64") {
    PluginLoader loader("../exprconv/exprconv.so");

    std::shared_ptr<DataConverter> conv(loader.getConverter("evaluate"));
    ConverterArgValues args(conv->getArgs());

    // the test pattern has its top bit set, so every reading below is negative
    SECTION("from four registers") {
        args.setArgValue("expression", "int64(R0, R1, R2, R3)");
        const ModbusRegisters input({TestNumbers::Int64::AB, TestNumbers::Int64::CD, TestNumbers::Int64::EF, TestNumbers::Int64::GH});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt64() == TestNumbers::Int64::ABCDEFGH_as_int64);
    }

    SECTION("from four registers byte swapped") {
        args.setArgValue("expression", "int64bs(R0, R1, R2, R3)");
        const ModbusRegisters input({TestNumbers::Int64::BA, TestNumbers::Int64::DC, TestNumbers::Int64::FE, TestNumbers::Int64::HG});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt64() == TestNumbers::Int64::ABCDEFGH_as_int64);
    }

    SECTION("from four registers in reverse word order") {
        args.setArgValue("expression", "int64(R3, R2, R1, R0)");
        const ModbusRegisters input({TestNumbers::Int64::GH, TestNumbers::Int64::EF, TestNumbers::Int64::CD, TestNumbers::Int64::AB});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt64() == TestNumbers::Int64::ABCDEFGH_as_int64);
    }

    SECTION("from four registers in reverse word order byte swapped") {
        args.setArgValue("expression", "int64bs(R3, R2, R1, R0)");
        const ModbusRegisters input({TestNumbers::Int64::HG, TestNumbers::Int64::FE, TestNumbers::Int64::DC, TestNumbers::Int64::BA});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt64() == TestNumbers::Int64::ABCDEFGH_as_int64);
    }

    SECTION("and publish the negative value") {
        args.setArgValue("expression", "int64(R0, R1, R2, R3)");
        const ModbusRegisters input({TestNumbers::Int64::AB, TestNumbers::Int64::CD, TestNumbers::Int64::EF, TestNumbers::Int64::GH});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getString() == std::to_string(TestNumbers::Int64::ABCDEFGH_as_int64));
    }
}

#endif
#endif
