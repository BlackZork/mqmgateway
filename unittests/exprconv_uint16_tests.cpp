#include <catch2/catch_all.hpp>
#include "libmodmqttconv/converterplugin.hpp"
#include "libmodmqttsrv/config.hpp"
#include "libmodmqttsrv/dll_import.hpp"

#include "testnumbers.hpp"
#include "plugin_utils.hpp"

#ifdef HAVE_EXPRTK

TEST_CASE ("exprtk should read uint16") {
    PluginLoader loader("../exprconv/exprconv.so");

    std::shared_ptr<DataConverter> conv(loader.getConverter("evaluate"));
    ConverterArgValues args(conv->getArgs());


    SECTION("from single register") {
        args.setArgValue("expression", "R0");
        const ModbusRegisters input(TestNumbers::Int::AB);

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt() == TestNumbers::Int::AB_as_uint16);
    }

    SECTION("from single register byte swapped") {
        args.setArgValue("expression", "uint16bs(R0)");
        const ModbusRegisters input(TestNumbers::Int::BA);

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt() == TestNumbers::Int::AB_as_uint16);
    }
}

TEST_CASE ("exprtk should write uint16") {
    PluginLoader loader("../exprconv/exprconv.so");

    std::shared_ptr<DataConverter> conv(loader.getConverter("evaluate"));

    MqttValue input = MqttValue::fromInt(TestNumbers::Int::AB_as_uint16);

    ConverterArgValues args(conv->getArgs());

    SECTION("to single register in AB format if no write helper is set") {

        args.setArgValue("expression", "M0");

        conv->setArgValues(args);
        const ModbusRegisters converted = conv->toModbus(input, 1);
        const ModbusRegisters expected({TestNumbers::Int::AB});

        REQUIRE(converted.values() == expected.values());
    }

    SECTION("to single register in AB format if uint16 helper is set") {

        args.setArgValue("expression", "M0");
        args.setArgValue("write_as", "uint16");

        conv->setArgValues(args);
        const ModbusRegisters converted = conv->toModbus(input, 1);
        const ModbusRegisters expected({TestNumbers::Int::AB});

        REQUIRE(converted.values() == expected.values());
    }


    SECTION("to single register in BA format") {

        args.setArgValue("expression", "M0");
        args.setArgValue("write_as", "uint16bs");

        conv->setArgValues(args);
        const ModbusRegisters converted = conv->toModbus(input, 1);
        const ModbusRegisters expected({TestNumbers::Int::BA});

        REQUIRE(converted.values() == expected.values());
    }
}

#endif
