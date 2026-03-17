#include <catch2/catch_all.hpp>
#include "libmodmqttconv/converterplugin.hpp"
#include "libmodmqttsrv/config.hpp"
#include "libmodmqttsrv/dll_import.hpp"

#include "testnumbers.hpp"
#include "plugin_utils.hpp"

#ifdef HAVE_EXPRTK

TEST_CASE ("exprtk should read int16") {
    PluginLoader loader("../exprconv/exprconv.so");

    std::shared_ptr<DataConverter> conv(loader.getConverter("evaluate"));
    ConverterArgValues args(conv->getArgs());


    SECTION("from single register") {
        args.setArgValue("expression", "int16(R0)");
        const ModbusRegisters input(TestNumbers::Int::AB);

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt() == TestNumbers::Int::AB_as_int16);
    }

    SECTION("from single register byte swapped") {
        args.setArgValue("expression", "int16bs(R0)");
        const ModbusRegisters input(TestNumbers::Int::AB);

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt() == TestNumbers::Int::AB_as_int16);
    }
}

TEST_CASE ("exprtk should write int16") {
    PluginLoader loader("../exprconv/exprconv.so");

    std::shared_ptr<DataConverter> conv(loader.getConverter("evaluate"));

    MqttValue input = MqttValue::fromInt(TestNumbers::Int::AB_as_int16);

    ConverterArgValues args(conv->getArgs());

    SECTION("to single register in AB format") {

        args.setArgValue("expression", "M0");
        args.setArgValue("write_as", "int16");

        conv->setArgValues(args);
        const ModbusRegisters converted = conv->toModbus(input, 1);
        const ModbusRegisters expected({TestNumbers::Int::AB});

        REQUIRE(converted.values() == expected.values());
    }

    SECTION("to single register in BA format") {

        args.setArgValue("expression", "M0");
        args.setArgValue("write_as", "int16bs");

        conv->setArgValues(args);
        const ModbusRegisters converted = conv->toModbus(input, 1);
        const ModbusRegisters expected({TestNumbers::Int::BA});

        REQUIRE(converted.values() == expected.values());
    }
}

#endif
