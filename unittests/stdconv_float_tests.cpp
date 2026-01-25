#include <libmodmqttsrv/config.hpp>
#include "catch2/catch_all.hpp"
#include "libmodmqttsrv/dll_import.hpp"

#include "libmodmqttconv/converterplugin.hpp"

#include "testnumbers.hpp"

TEST_CASE("A float32 value should be read") {
    std::string stdconv_path = "../stdconv/stdconv.so";

    std::shared_ptr<ConverterPlugin> plugin = modmqttd::dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin"
    );
    std::shared_ptr<DataConverter> conv(plugin->getConverter("float32"));
    ConverterArgValues args(conv->getArgs());

    SECTION("when two registers contains a float ABCD format") {
        const ModbusRegisters input({TestNumbers::Float::AB, TestNumbers::Float::CD});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getDouble() == TestNumbers::Float::ABCD_as_float);

        REQUIRE_THAT(output.getDouble(), Catch::Matchers::WithinULP(TestNumbers::Float::ABCD_as_float, 0));
        REQUIRE(output.getString() == "-1.234567");
    }

    SECTION("when two registers contains a float CDAB format") {
        const ModbusRegisters input({TestNumbers::Float::CD, TestNumbers::Float::AB});

        args.setArgValue(ConverterArg::sLowFirstArgName, ConverterArgType::BOOL, "true");
        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE_THAT(output.getDouble(), Catch::Matchers::WithinULP(TestNumbers::Float::ABCD_as_float, 0));
        REQUIRE(output.getString() == std::to_string(TestNumbers::Float::ABCD_as_float));
    }

    SECTION("when two registers contains a float BADC format") {
        const ModbusRegisters input({TestNumbers::Float::BA, TestNumbers::Float::DC});

        args.setArgValue(ConverterArg::sSwapBytesArgName, ConverterArgType::BOOL, "true");
        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE_THAT(output.getDouble(), Catch::Matchers::WithinULP(TestNumbers::Float::ABCD_as_float, 0));
        REQUIRE(output.getString() == std::to_string(TestNumbers::Float::ABCD_as_float));
    }


    SECTION("when two registers contains a float DCBA format") {
        const ModbusRegisters input({TestNumbers::Float::DC, TestNumbers::Float::BA});

        args.setArgValue(ConverterArg::sLowFirstArgName, ConverterArgType::BOOL, "true");
        args.setArgValue(ConverterArg::sSwapBytesArgName, ConverterArgType::BOOL, "true");
        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE_THAT(output.getDouble(), Catch::Matchers::WithinULP(TestNumbers::Float::ABCD_as_float, 0));
        REQUIRE(output.getString() == std::to_string(TestNumbers::Float::ABCD_as_float));
    }

    SECTION("when precision is set") {
        const ModbusRegisters input({0xc2f6, 0xe979});

        args.setArgValue(ConverterArg::sPrecisionArgName, ConverterArgType::INT, "2");
        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);


        REQUIRE(output.getString() == "-123.46");
    }

}

TEST_CASE("A float32 value should be written") {
    std::string stdconv_path = "../stdconv/stdconv.so";

    std::shared_ptr<ConverterPlugin> plugin = modmqttd::dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin"
    );
    std::shared_ptr<DataConverter> conv(plugin->getConverter("float32"));
    ConverterArgValues args(conv->getArgs());

    std::string val("-1.234567");
    MqttValue input = MqttValue::fromBinary(val.c_str(), val.length());

    SECTION("to two registers in ABCD format") {

        conv->setArgValues(args);
        const ModbusRegisters converted = conv->toModbus(input, 2);
        const ModbusRegisters expected({TestNumbers::Float::AB, TestNumbers::Float::CD});

        REQUIRE(converted.values() == expected.values());
    }

    SECTION("to two registers in CDAB format") {
        args.setArgValue(ConverterArg::sLowFirstArgName, ConverterArgType::BOOL, "true");
        conv->setArgValues(args);
        const ModbusRegisters converted = conv->toModbus(input, 2);
        const ModbusRegisters expected({TestNumbers::Float::CD, TestNumbers::Float::AB});

        REQUIRE(converted.values() == expected.values());
    }

    SECTION("to two registers in BADC format") {
        args.setArgValue(ConverterArg::sSwapBytesArgName, ConverterArgType::BOOL, "true");
        conv->setArgValues(args);
        const ModbusRegisters converted = conv->toModbus(input, 2);
        const ModbusRegisters expected({TestNumbers::Float::BA, TestNumbers::Float::DC});

        REQUIRE(converted.values() == expected.values());
    }

    SECTION("to two registers in DCBA format") {
        args.setArgValue(ConverterArg::sLowFirstArgName, ConverterArgType::BOOL, "true");
        args.setArgValue(ConverterArg::sSwapBytesArgName, ConverterArgType::BOOL, "true");
        conv->setArgValues(args);
        const ModbusRegisters converted = conv->toModbus(input, 2);
        const ModbusRegisters expected({TestNumbers::Float::DC, TestNumbers::Float::BA});

        REQUIRE(converted.values() == expected.values());
    }
}
