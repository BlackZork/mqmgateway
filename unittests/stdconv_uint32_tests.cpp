#include <libmodmqttsrv/config.hpp>
#include <catch2/catch_all.hpp>
#include "libmodmqttsrv/dll_import.hpp"

#include "libmodmqttconv/converterplugin.hpp"
#include "libmodmqttconv/convexception.hpp"

#include "testnumbers.hpp"
#include "plugin_utils.hpp"

TEST_CASE("when std.uint32") {
    PluginLoader loader("../stdconv/stdconv.so");

    std::shared_ptr<DataConverter> conv(loader.getConverter("uint32"));
    ConverterArgValues args(conv->getArgs());

    SECTION("converts two modbus registers (high, low)") {
        ModbusRegisters input({TestNumbers::Int::AB,TestNumbers::Int::CD});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt64() == TestNumbers::Int::ABCD_as_uint32);
    }

    SECTION("converts two modbus registers (high, low) with byte swap") {
        args.setArgValue(ConverterArg::sSwapBytesArgName, "true");

        ModbusRegisters input({TestNumbers::Int::BA, TestNumbers::Int::DC});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt64() == TestNumbers::Int::ABCD_as_uint32);
    }

    SECTION("converts two modbus registers (low, high)") {
        args.setArgValue(ConverterArg::sLowFirstArgName, "true");

        ModbusRegisters input({TestNumbers::Int::AB,TestNumbers::Int::CD});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt64() == TestNumbers::Int::CDAB_as_uint32);
    }

    SECTION("converts two modbus registers (low, high) with byte swap") {
        args.setArgValue(ConverterArg::sLowFirstArgName, "true");
        args.setArgValue(ConverterArg::sSwapBytesArgName, "true");

        ModbusRegisters input({TestNumbers::Int::BA, TestNumbers::Int::DC});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt64() == TestNumbers::Int::CDAB_as_uint32);
    }

    SECTION("converts a single modbus register") {
        ModbusRegisters input(TestNumbers::Int::AB);

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt64() == TestNumbers::Int::AB_as_uint16);
    }

    SECTION("keeps a value above INT32_MAX unsigned") {
        ModbusRegisters input({0xffff, 0xffff});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getString() == "4294967295");
    }

    SECTION("writes to two modbus registers (high, low)") {
        MqttValue input = MqttValue::fromInt64(TestNumbers::Int::ABCD_as_uint32);

        conv->setArgValues(args);
        ModbusRegisters output = conv->toModbus(input, 2);

        REQUIRE(output.getValue(0) == TestNumbers::Int::AB);
        REQUIRE(output.getValue(1) == TestNumbers::Int::CD);
    }

    SECTION("writes to two modbus registers (high, low) with byte swap") {
        args.setArgValue(ConverterArg::sSwapBytesArgName, "true");

        MqttValue input = MqttValue::fromInt64(TestNumbers::Int::ABCD_as_uint32);

        conv->setArgValues(args);
        ModbusRegisters output = conv->toModbus(input, 2);

        REQUIRE(output.getValue(0) == TestNumbers::Int::BA);
        REQUIRE(output.getValue(1) == TestNumbers::Int::DC);
    }

    SECTION("writes to two modbus registers (low, high)") {
        args.setArgValue(ConverterArg::sLowFirstArgName, "true");

        MqttValue input = MqttValue::fromInt64(TestNumbers::Int::ABCD_as_uint32);

        conv->setArgValues(args);
        ModbusRegisters output = conv->toModbus(input, 2);

        REQUIRE(output.getValue(0) == TestNumbers::Int::CD);
        REQUIRE(output.getValue(1) == TestNumbers::Int::AB);
    }

    SECTION("writes to two modbus registers (low, high) with byte swap") {
        args.setArgValue(ConverterArg::sLowFirstArgName, "true");
        args.setArgValue(ConverterArg::sSwapBytesArgName, "true");

        MqttValue input = MqttValue::fromInt64(TestNumbers::Int::ABCD_as_uint32);

        conv->setArgValues(args);
        ModbusRegisters output = conv->toModbus(input, 2);

        REQUIRE(output.getValue(0) == TestNumbers::Int::DC);
        REQUIRE(output.getValue(1) == TestNumbers::Int::BA);
    }

    SECTION("restores the default word order when low_first is set back to false") {
        ModbusRegisters input({TestNumbers::Int::AB, TestNumbers::Int::CD});

        args.setArgValue(ConverterArg::sLowFirstArgName, "true");
        conv->setArgValues(args);
        REQUIRE(conv->toMqtt(input).getInt64() == TestNumbers::Int::CDAB_as_uint32);

        args.setArgValue(ConverterArg::sLowFirstArgName, "false");
        conv->setArgValues(args);
        REQUIRE(conv->toMqtt(input).getInt64() == TestNumbers::Int::ABCD_as_uint32);
    }
}
