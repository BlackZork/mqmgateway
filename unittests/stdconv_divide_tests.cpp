#include <libmodmqttsrv/config.hpp>
#include "catch2/catch_all.hpp"
#include "libmodmqttsrv/dll_import.hpp"

#include "libmodmqttconv/converterplugin.hpp"

#include "testnumbers.hpp"
#include "plugin_utils.hpp"

TEST_CASE("A number value should be divided") {
    PluginLoader loader("../stdconv/stdconv.so");

    std::shared_ptr<DataConverter> conv(loader.getConverter("divide"));

    ConverterArgValues args(conv->getArgs());
    args.setArgValue("divisor", "2.0");

    SECTION("when one int32 value is converted to two modbus registers, high byte first") {
        MqttValue input(0x20004);

        conv->setArgValues(args);
        ModbusRegisters output = conv->toModbus(input, 2);

        REQUIRE(output.values() == std::vector<uint16_t>({0x1, 0x2}));
    }


    SECTION("when one int32 value is converted to two modbus registers, low byte first") {
        MqttValue input(0x20004);

        args.setArgValue(ConverterArg::sPrecisionArgName, "2");
        args.setArgValue(ConverterArg::sLowFirstArgName, "true");

        conv->setArgValues(args);
        ModbusRegisters output = conv->toModbus(input, 2);

        REQUIRE(output.values() == std::vector<uint16_t>({0x2, 0x1}));
    }

    SECTION("when one int32 byte swapped value is converted to two modbus registers, low byte first") {
        MqttValue input(TestNumbers::Int::ABCD_as_int32);

        args.setArgValue(ConverterArg::sPrecisionArgName, "0");
        args.setArgValue(ConverterArg::sLowFirstArgName, "true");
        args.setArgValue(ConverterArg::sSwapBytesArgName, "true");

        conv->setArgValues(args);
        ModbusRegisters output = conv->toModbus(input, 2);

        REQUIRE(output.values() == std::vector<uint16_t>({0xea61, 0xd9d0}));
    }

    SECTION("when two modbus registers are converted into a string, high byte first") {
        ModbusRegisters input = ModbusRegisters(std::vector<uint16_t>{0x01, 0x2});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getString() == "32769");
    }


    SECTION("when two modbus registers are converted into a string, low byte first") {
        ModbusRegisters input = ModbusRegisters(std::vector<uint16_t>{0x02, 0x1});

        args.setArgValue(ConverterArg::sPrecisionArgName, "0");
        args.setArgValue(ConverterArg::sLowFirstArgName, "true");

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getString() == "32769");
    }


    SECTION("when two modbus byte swapped registers are converted into a string, low byte first") {
        ModbusRegisters input = ModbusRegisters(std::vector<uint16_t>{0x0201, 0x0804});

        args.setArgValue(ConverterArg::sPrecisionArgName, "0");
        args.setArgValue(ConverterArg::sLowFirstArgName, "true");
        args.setArgValue(ConverterArg::sSwapBytesArgName, "true");

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt() == 0x02040081);
    }



    SECTION("when single modbus register is converted into a string") {
        ModbusRegisters input = ModbusRegisters(std::vector<uint16_t>{0x02});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getString() == "1");
    }


    SECTION("mqtt value is converted to single modbus register") {
        MqttValue input(0x4);

        conv->setArgValues(args);
        ModbusRegisters output = conv->toModbus(input, 1);

        REQUIRE(output.values() == std::vector<uint16_t>({0x2}));
    }
}


TEST_CASE("When precision of divided mqtt value ") {
    PluginLoader loader("../stdconv/stdconv.so");

    std::shared_ptr<DataConverter> conv(loader.getConverter("divide"));
    ConverterArgValues args(conv->getArgs());
    args.setArgValue("divisor", "3.0");

    SECTION("is default 6 then fractional part contains 6 digits") {
        ModbusRegisters input = ModbusRegisters(std::vector<uint16_t>{10});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getString() == "3.333333");
    }

    SECTION("is 3 then fractional part contains 3 digits") {
        ModbusRegisters input = ModbusRegisters(std::vector<uint16_t>{10});

        args.setArgValue(ConverterArg::sPrecisionArgName, "3");
        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getString() == "3.333");
    }

    SECTION("is 0 then there is no fractional part") {
        ModbusRegisters input = ModbusRegisters(std::vector<uint16_t>{10});

        args.setArgValue(ConverterArg::sPrecisionArgName, "0");
        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getString() == "3");
    }
}
