#include <libmodmqttsrv/config.hpp>
#include "catch2/catch_all.hpp"
#include "libmodmqttsrv/dll_import.hpp"


#include "libmodmqttconv/converterplugin.hpp"
#include "plugin_utils.hpp"

TEST_CASE ("An instance of map converter") {
    PluginLoader loader("../stdconv/stdconv.so");

    std::shared_ptr<DataConverter> conv(loader.getConverter("map"));
    ConverterArgValues args(conv->getArgs());

    SECTION("with int value") {
        args.setArgValue("map", "{3: 38400}");

        conv->setArgValues(args);

        SECTION("should convert register value to mapped int") {
            ModbusRegisters data(3);

            MqttValue ret = conv->toMqtt(data);

            REQUIRE(ret.getString() == "38400");
        }

        SECTION("should convert int mqtt value to register data") {
            MqttValue val(MqttValue::fromInt(38400));

            ModbusRegisters ret = conv->toModbus(val, 1);
            REQUIRE(ret.getValue(0) == 3);
        }

        SECTION("should pass unmapped mqtt value") {
            MqttValue val(MqttValue::fromInt(12));

            ModbusRegisters ret = conv->toModbus(val, 1);
            REQUIRE(ret.getValue(0) == 12);
        }

        SECTION("should pass unmapped modbus value") {
            MqttValue val(MqttValue::fromInt(17));

            ModbusRegisters ret = conv->toModbus(val, 1);
            REQUIRE(ret.getValue(0) == 17);
        }
    }

    SECTION("with string value") {
        args.setArgValue("map", "{1: \"one\"}");
        conv->setArgValues(args);

        SECTION("should convert register value to mapped string") {
            ModbusRegisters data(1);
            MqttValue ret = conv->toMqtt(data);

            REQUIRE(ret.getString() == "one");
        }

        SECTION("should convert string mqtt value to register data") {
            MqttValue val(MqttValue::fromString("one"));

            ModbusRegisters ret = conv->toModbus(val, 1);
            REQUIRE(ret.getValue(0) == 1);
        }
    }

    SECTION("with string value, spaces and escaped chars") {
        args.setArgValue("map", "{ 1 : \" \\\\one \"  }");

        conv->setArgValues(args);

        SECTION("should convert register value to mapped string") {
            ModbusRegisters data(1);
            MqttValue ret = conv->toMqtt(data);

            REQUIRE(ret.getString() == " \\one ");
        }
    }

    SECTION("with colon in value") {
        args.setArgValue("map", "{1:\":\"}");
        conv->setArgValues(args);

        SECTION("should convert register value to mapped string") {
            ModbusRegisters data(1);
            MqttValue ret = conv->toMqtt(data);

            REQUIRE(ret.getString() == ":");
        }
    }

    SECTION("with multiple mappings") {
        args.setArgValue("map", "{1:11,2:\"two\"}");
        conv->setArgValues(args);

        SECTION("should use first mapping") {
            ModbusRegisters data(1);
            MqttValue ret = conv->toMqtt(data);
            REQUIRE(ret.getString() == "11");
        }

        SECTION("should use second mapping") {
            ModbusRegisters data(2);
            MqttValue ret = conv->toMqtt(data);
            REQUIRE(ret.getString() == "two");
        }
    }

    SECTION("with register in hex format") {
        args.setArgValue("map", "{0x11:17}");
        conv->setArgValues(args);

        SECTION("should convert register value to mapped string") {
            ModbusRegisters data(0x11);
            MqttValue ret = conv->toMqtt(data);

            REQUIRE(ret.getString() == "17");
        }

        SECTION("should convert int mqtt value to register data") {
            MqttValue val(MqttValue::fromInt(17));

            ModbusRegisters ret = conv->toModbus(val, 1);
            REQUIRE(ret.getValue(0) == 0x11);
        }

    }

    SECTION("with string map with two keys") {
        args.setArgValue("map", "{24:\"t\", 1:\"o\"}");
        conv->setArgValues(args);

        SECTION("should convert register value to mapped string") {
            ModbusRegisters data(24);
            MqttValue ret = conv->toMqtt(data);

            REQUIRE(ret.getString() == "t");
        }
    }

    SECTION("with string map without braces") {
        args.setArgValue("map", "24:\"t\", 1:\"o\"");
        conv->setArgValues(args);

        SECTION("should convert register value to mapped string") {
            ModbusRegisters data(24);
            MqttValue ret = conv->toMqtt(data);

            REQUIRE(ret.getString() == "t");
        }
    }

    SECTION("with int map without braces") {
        args.setArgValue("map", "24:1, 1:2");
        conv->setArgValues(args);

        SECTION("should convert register value to mapped string") {
            ModbusRegisters data(24);
            MqttValue ret = conv->toMqtt(data);

            REQUIRE(ret.getString() == "1");
        }
    }

    SECTION("with string map with int values") {
        args.setArgValue("map", "24:\"1\",1:\"2\"");
        conv->setArgValues(args);

        SECTION("should convert register value to mapped string") {
            ModbusRegisters data(24);
            MqttValue ret = conv->toMqtt(data);

            REQUIRE(ret.getString() == "1");
        }
    }

    SECTION("with space between string value and closing brace") {
        args.setArgValue("map", R"({ 0:"nnnn", 1:"nnn.n" })");
        conv->setArgValues(args);

        SECTION("should not add spurious key=0 when parsing closing brace") {
            ModbusRegisters data(0);
            MqttValue ret = conv->toMqtt(data);

            REQUIRE(ret.getString() == "nnnn");
        }
    }

    SECTION("with int value bigger than 32678") {
        args.setArgValue("map", R"({ 0:"DI1", 1:"DI2", 32768:"AI1", 32769:"AI2" })");
        conv->setArgValues(args);

        SECTION("should convert value to string") {
            ModbusRegisters data(32769);
            MqttValue ret = conv->toMqtt(data);

            REQUIRE(ret.getString() == "AI2");
        }
    }
}
