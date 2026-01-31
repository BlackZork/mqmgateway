#include <catch2/catch_all.hpp>
#include "libmodmqttconv/converterplugin.hpp"
#include "libmodmqttsrv/dll_import.hpp"

#include "testnumbers.hpp"
#include "plugin_utils.hpp"

#ifdef HAVE_EXPRTK

TEST_CASE("exprtk should read float from two registers") {
    PluginLoader loader("../exprconv/exprconv.so");

    std::shared_ptr<DataConverter> conv(loader.getConverter("evaluate"));

    const float expected = -123.456f; // 0xc2f6e979 in IEEE 754 hex representation
    const std::string expectedString = "-123.456001";
    ConverterArgValues args(conv->getArgs());

    SECTION("when byte order is ABCD") {

        args.setArgValue("expression", "flt32(R0, R1)");
        const ModbusRegisters input({TestNumbers::Float::AB, TestNumbers::Float::CD});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE_THAT(output.getDouble(), Catch::Matchers::WithinULP(TestNumbers::Float::ABCD_as_float, 0));
        REQUIRE(output.getString() == std::to_string(TestNumbers::Float::ABCD_as_float));
    }

    SECTION("and byte order is CDAB") {
        args.setArgValue("expression", "flt32(R1, R0)");
        const ModbusRegisters input({TestNumbers::Float::AB, TestNumbers::Float::CD});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE_THAT(output.getDouble(), Catch::Matchers::WithinULP(TestNumbers::Float::CDAB_as_float, 0));
        REQUIRE(output.getString() == std::to_string(TestNumbers::Float::CDAB_as_float));
    }

    SECTION("and byte order is BADC") {
        args.setArgValue("expression", "flt32bs(R0, R1)");
        const ModbusRegisters input({TestNumbers::Float::BA, TestNumbers::Float::DC});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE_THAT(output.getDouble(), Catch::Matchers::WithinULP(TestNumbers::Float::ABCD_as_float, 0));
        REQUIRE(output.getString() == std::to_string(TestNumbers::Float::ABCD_as_float));
    }

    SECTION("and byte order is DCBA") {
        args.setArgValue("expression", "flt32bs(R1, R0)");
        const ModbusRegisters input({TestNumbers::Float::BA, TestNumbers::Float::DC});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE_THAT(output.getDouble(), Catch::Matchers::WithinULP(TestNumbers::Float::CDAB_as_float, 0));
        REQUIRE(output.getString() == std::to_string(TestNumbers::Float::CDAB_as_float));
    }

    SECTION("and precision is set") {
        args.setArgValue("expression", "flt32(R0, R1)");
        args.setArgValue(ConverterArg::sPrecisionArgName, "3");
        const ModbusRegisters input({0xc2f6, 0xe979});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getString() == "-123.456");
    }
}

TEST_CASE("exprtk should write float") {
    PluginLoader loader("../exprconv/exprconv.so");

    std::shared_ptr<DataConverter> conv(loader.getConverter("evaluate"));

    std::string val("-1.234567");
    MqttValue input = MqttValue::fromBinary(val.c_str(), val.length());

    ConverterArgValues args(conv->getArgs());

    SECTION("to two registers in ABCD format") {

        args.setArgValue("expression", "M0*1");
        args.setArgValue("write_as", "flt32");

        conv->setArgValues(args);
        const ModbusRegisters converted = conv->toModbus(input, 2);
        const ModbusRegisters expected({TestNumbers::Float::AB, TestNumbers::Float::CD});

        REQUIRE(converted.values() == expected.values());
    }
}

#endif
