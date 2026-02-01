#include <catch2/catch_all.hpp>
#include "libmodmqttconv/converterplugin.hpp"
#include "libmodmqttsrv/config.hpp"
#include "libmodmqttsrv/dll_import.hpp"

#include "testnumbers.hpp"
#include "plugin_utils.hpp"

#ifdef HAVE_EXPRTK

TEST_CASE ("exprtk should read uint32") {
    PluginLoader loader("../exprconv/exprconv.so");

    std::shared_ptr<DataConverter> conv(loader.getConverter("evaluate"));
    ConverterArgValues args(conv->getArgs());

    SECTION("from two registers") {
        args.setArgValue("expression", "uint32(R0, R1)");
        const ModbusRegisters input({TestNumbers::Int::AB, TestNumbers::Int::CD});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt64() == TestNumbers::Int::ABCD_as_uint32);
    }

    SECTION("from two registers byte swapped") {
        args.setArgValue("expression", "uint32bs(R0, R1)");
        const ModbusRegisters input({TestNumbers::Int::BA, TestNumbers::Int::DC});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt64() == TestNumbers::Int::ABCD_as_uint32);
    }
}

TEST_CASE("exprtk should write uint32") {
    PluginLoader loader("../exprconv/exprconv.so");

    std::shared_ptr<DataConverter> conv(loader.getConverter("evaluate"));

    MqttValue input = MqttValue::fromInt(TestNumbers::Int::ABCD_as_uint32);

    ConverterArgValues args(conv->getArgs());

    SECTION("to two registers in ABCD format") {

        args.setArgValue("expression", "M0*1");
        args.setArgValue("write_as", "uint32");

        conv->setArgValues(args);
        const ModbusRegisters converted = conv->toModbus(input, 2);
        const ModbusRegisters expected({TestNumbers::Int::AB, TestNumbers::Int::CD});

        REQUIRE(converted.values() == expected.values());
    }


    SECTION("to two registers in CDAB format") {

        args.setArgValue("expression", "M0*1");
        args.setArgValue("write_as", "uint32");
        args.setArgValue("low_first", "true");

        conv->setArgValues(args);
        const ModbusRegisters converted = conv->toModbus(input, 2);
        const ModbusRegisters expected({TestNumbers::Int::CD, TestNumbers::Int::AB});

        REQUIRE(converted.values() == expected.values());
    }


    SECTION("to two registers in BADC format") {

        args.setArgValue("expression", "M0*1");
        args.setArgValue("write_as", "uint32bs");

        conv->setArgValues(args);
        const ModbusRegisters converted = conv->toModbus(input, 2);
        const ModbusRegisters expected({TestNumbers::Int::BA, TestNumbers::Int::DC});

        REQUIRE(converted.values() == expected.values());
    }


    SECTION("to two registers in DCBA format") {

        args.setArgValue("expression", "M0*1");
        args.setArgValue("write_as", "uint32bs");
        args.setArgValue("low_first", "true");

        conv->setArgValues(args);
        const ModbusRegisters converted = conv->toModbus(input, 2);
        const ModbusRegisters expected({TestNumbers::Int::DC, TestNumbers::Int::BA});

        REQUIRE(converted.values() == expected.values());
    }
}

#endif
