#include <libmodmqttsrv/config.hpp>
#include <catch2/catch_all.hpp>
#include "libmodmqttsrv/dll_import.hpp"

#include "libmodmqttconv/converterplugin.hpp"
#include "libmodmqttconv/convexception.hpp"

#include "testnumbers.hpp"
#include "plugin_utils.hpp"

TEST_CASE("when std.int32") {
    PluginLoader loader("../stdconv/stdconv.so");

    std::shared_ptr<DataConverter> conv(loader.getConverter("int32"));
    ConverterArgValues args(conv->getArgs());

    SECTION("converts two modbus registers (high, low)") {

        ModbusRegisters input({TestNumbers::Int::AB,TestNumbers::Int::CD});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt() == TestNumbers::Int::ABCD_as_int32);
    }

    SECTION("converts two modbus registers (high, low) with byte swap") {
        args.setArgValue(ConverterArg::sSwapBytesArgName, "true");

        ModbusRegisters input({TestNumbers::Int::BA,TestNumbers::Int::DC});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt() == TestNumbers::Int::ABCD_as_int32);
    }


    SECTION("converts two modbus registers (low, high)") {
        args.setArgValue(ConverterArg::sLowFirstArgName, "true");

        ModbusRegisters input({TestNumbers::Int::AB,TestNumbers::Int::CD});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt() == TestNumbers::Int::CDAB_as_int32);
    }

    SECTION("converts two modbus registers (low, high) with byte swap") {
        args.setArgValue(ConverterArg::sLowFirstArgName, "true");
        args.setArgValue(ConverterArg::sSwapBytesArgName, "true");

        ModbusRegisters input({TestNumbers::Int::BA,TestNumbers::Int::DC});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt() == TestNumbers::Int::CDAB_as_int32);
    }

    // TODO we should output warning in this case, this looks like configuration error
    SECTION("converts a single modbus register ignoring low_first arg") {
        args.setArgValue(ConverterArg::sLowFirstArgName, "true");

        ModbusRegisters input({TestNumbers::Int::AB});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt() == TestNumbers::Int::AB);
    }

    SECTION("writes to two modbus registers (low, high)") {
        args.setArgValue(ConverterArg::sLowFirstArgName, "true");

        MqttValue input(TestNumbers::Int::ABCD_as_int32);

        conv->setArgValues(args);
        ModbusRegisters output = conv->toModbus(input, 2);

        REQUIRE(output.getValue(0) == TestNumbers::Int::CD);
        REQUIRE(output.getValue(1) == TestNumbers::Int::AB);
    }

    SECTION("writes to two modbus registers (low, high) with byte swap") {
        args.setArgValue(ConverterArg::sLowFirstArgName, "true");
        args.setArgValue(ConverterArg::sSwapBytesArgName, "true");

        MqttValue input(TestNumbers::Int::ABCD_as_int32);

        conv->setArgValues(args);
        ModbusRegisters output = conv->toModbus(input, 2);

        REQUIRE(output.getValue(0) == TestNumbers::Int::DC);
        REQUIRE(output.getValue(1) == TestNumbers::Int::BA);
    }


    SECTION("writes to two modbus registers (high, low)") {
        MqttValue input(TestNumbers::Int::ABCD_as_int32);

        conv->setArgValues(args);
        ModbusRegisters output = conv->toModbus(input, 2);

        REQUIRE(output.getValue(0) == TestNumbers::Int::AB);
        REQUIRE(output.getValue(1) == TestNumbers::Int::CD);
    }

    SECTION("writes to two modbus registers (high, low) with byte swap") {
        args.setArgValue(ConverterArg::sSwapBytesArgName, "true");

        MqttValue input(TestNumbers::Int::ABCD_as_int32);

        conv->setArgValues(args);
        ModbusRegisters output = conv->toModbus(input, 2);

        REQUIRE(output.getValue(0) == TestNumbers::Int::BA);
        REQUIRE(output.getValue(1) == TestNumbers::Int::DC);
    }

}


