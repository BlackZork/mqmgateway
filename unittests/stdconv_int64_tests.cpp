#include <libmodmqttsrv/config.hpp>
#include <catch2/catch_all.hpp>
#include "libmodmqttsrv/dll_import.hpp"

#include "libmodmqttconv/converterplugin.hpp"
#include "libmodmqttconv/convexception.hpp"

#include "testnumbers.hpp"
#include "plugin_utils.hpp"

TEST_CASE("when std.int64") {
    PluginLoader loader("../stdconv/stdconv.so");

    std::shared_ptr<DataConverter> conv(loader.getConverter("int64"));
    ConverterArgValues args(conv->getArgs());

    SECTION("converts four modbus registers (high to low)") {
        ModbusRegisters input({TestNumbers::Int64::AB, TestNumbers::Int64::CD, TestNumbers::Int64::EF, TestNumbers::Int64::GH});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt64() == TestNumbers::Int64::ABCDEFGH_as_int64);
    }

    SECTION("converts four modbus registers (high to low) with byte swap") {
        args.setArgValue(ConverterArg::sSwapBytesArgName, "true");

        ModbusRegisters input({TestNumbers::Int64::BA, TestNumbers::Int64::DC, TestNumbers::Int64::FE, TestNumbers::Int64::HG});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt64() == TestNumbers::Int64::ABCDEFGH_as_int64);
    }

    SECTION("converts four modbus registers (low to high)") {
        args.setArgValue(ConverterArg::sLowFirstArgName, "true");

        ModbusRegisters input({TestNumbers::Int64::AB, TestNumbers::Int64::CD, TestNumbers::Int64::EF, TestNumbers::Int64::GH});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt64() == TestNumbers::Int64::GHEFCDAB_as_int64);
    }

    SECTION("converts four modbus registers (low to high) with byte swap") {
        args.setArgValue(ConverterArg::sLowFirstArgName, "true");
        args.setArgValue(ConverterArg::sSwapBytesArgName, "true");

        ModbusRegisters input({TestNumbers::Int64::BA, TestNumbers::Int64::DC, TestNumbers::Int64::FE, TestNumbers::Int64::HG});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt64() == TestNumbers::Int64::GHEFCDAB_as_int64);
    }

    SECTION("converts fewer registers without sign extension") {
        ModbusRegisters input({TestNumbers::Int::AB, TestNumbers::Int::CD});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt64() == TestNumbers::Int::ABCD_as_uint32);
    }

    SECTION("writes to four modbus registers (high to low)") {
        MqttValue input = MqttValue::fromInt64(TestNumbers::Int64::ABCDEFGH_as_int64);

        conv->setArgValues(args);
        ModbusRegisters output = conv->toModbus(input, 4);

        REQUIRE(output.values() == std::vector<uint16_t>({TestNumbers::Int64::AB, TestNumbers::Int64::CD, TestNumbers::Int64::EF, TestNumbers::Int64::GH}));
    }

    SECTION("writes to four modbus registers (high to low) with byte swap") {
        args.setArgValue(ConverterArg::sSwapBytesArgName, "true");

        MqttValue input = MqttValue::fromInt64(TestNumbers::Int64::ABCDEFGH_as_int64);

        conv->setArgValues(args);
        ModbusRegisters output = conv->toModbus(input, 4);

        REQUIRE(output.values() == std::vector<uint16_t>({TestNumbers::Int64::BA, TestNumbers::Int64::DC, TestNumbers::Int64::FE, TestNumbers::Int64::HG}));
    }

    SECTION("writes to four modbus registers (low to high)") {
        args.setArgValue(ConverterArg::sLowFirstArgName, "true");

        MqttValue input = MqttValue::fromInt64(TestNumbers::Int64::ABCDEFGH_as_int64);

        conv->setArgValues(args);
        ModbusRegisters output = conv->toModbus(input, 4);

        REQUIRE(output.values() == std::vector<uint16_t>({TestNumbers::Int64::GH, TestNumbers::Int64::EF, TestNumbers::Int64::CD, TestNumbers::Int64::AB}));
    }

    SECTION("writes to four modbus registers (low to high) with byte swap") {
        args.setArgValue(ConverterArg::sLowFirstArgName, "true");
        args.setArgValue(ConverterArg::sSwapBytesArgName, "true");

        MqttValue input = MqttValue::fromInt64(TestNumbers::Int64::ABCDEFGH_as_int64);

        conv->setArgValues(args);
        ModbusRegisters output = conv->toModbus(input, 4);

        REQUIRE(output.values() == std::vector<uint16_t>({TestNumbers::Int64::HG, TestNumbers::Int64::FE, TestNumbers::Int64::DC, TestNumbers::Int64::BA}));
    }
}
