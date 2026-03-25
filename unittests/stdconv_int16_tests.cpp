#include <libmodmqttsrv/config.hpp>
#include "catch2/catch_all.hpp"
#include "libmodmqttsrv/dll_import.hpp"

#include "libmodmqttconv/converterplugin.hpp"
#include "libmodmqttconv/convexception.hpp"

#include "testnumbers.hpp"
#include "plugin_utils.hpp"

TEST_CASE("std int16 converter") {
    PluginLoader loader("../stdconv/stdconv.so");

    std::shared_ptr<DataConverter> conv(loader.getConverter("int16"));
    ConverterArgValues args(conv->getArgs());

    SECTION("should read") {
        SECTION("single register as int16") {

            ModbusRegisters input({TestNumbers::Int::AB});

            conv->setArgValues(args);
            MqttValue output = conv->toMqtt(input);

            REQUIRE(output.getInt() == TestNumbers::Int::AB_as_int16);
        }

        SECTION("single register as int16 byteswapped") {
            args.setArgValue(ConverterArg::sSwapBytesArgName, "true");

            ModbusRegisters input({TestNumbers::Int::BA});

            conv->setArgValues(args);
            MqttValue output = conv->toMqtt(input);

            REQUIRE(output.getInt() == TestNumbers::Int::AB_as_int16);
        }
    }


    SECTION("should write") {
        MqttValue input = MqttValue::fromInt(TestNumbers::Int::AB_as_int16);

        SECTION("to single register in AB format") {
            conv->setArgValues(args);
            ModbusRegisters output = conv->toModbus(input, 1);

            REQUIRE(output.getValue(0) == TestNumbers::Int::AB);
        }

        SECTION("to single register in BA format") {
            args.setArgValue(ConverterArg::sSwapBytesArgName, "true");
            conv->setArgValues(args);
            ModbusRegisters output = conv->toModbus(input, 1);

            REQUIRE(output.getValue(0) == TestNumbers::Int::BA);
        }

    }
}

TEST_CASE("std uint16 converter") {
    PluginLoader loader("../stdconv/stdconv.so");

    std::shared_ptr<DataConverter> conv(loader.getConverter("uint16"));
    ConverterArgValues args(conv->getArgs());

    SECTION("should read") {
        SECTION("single register as uint16") {

            ModbusRegisters input({TestNumbers::Int::AB});

            conv->setArgValues(args);
            MqttValue output = conv->toMqtt(input);

            REQUIRE(output.getInt() == TestNumbers::Int::AB_as_uint16);
        }

        SECTION("single register as uint16 byteswapped") {
            args.setArgValue(ConverterArg::sSwapBytesArgName, "true");

            ModbusRegisters input({TestNumbers::Int::BA});

            conv->setArgValues(args);
            MqttValue output = conv->toMqtt(input);

            REQUIRE(output.getInt() == TestNumbers::Int::AB_as_uint16);
        }
    }


    SECTION("should write") {
        MqttValue input = MqttValue::fromInt(TestNumbers::Int::AB_as_uint16);

        SECTION("to single register in AB format") {
            conv->setArgValues(args);
            ModbusRegisters output = conv->toModbus(input, 1);

            REQUIRE(output.getValue(0) == TestNumbers::Int::AB);
        }

        SECTION("to single register in BA format") {
            args.setArgValue(ConverterArg::sSwapBytesArgName, "true");
            conv->setArgValues(args);
            ModbusRegisters output = conv->toModbus(input, 1);

            REQUIRE(output.getValue(0) == TestNumbers::Int::BA);
        }

    }
}
