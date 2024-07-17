#include <libmodmqttsrv/config.hpp>
#include "catch2/catch_all.hpp"
#include <boost/dll/import.hpp>

#include "libmodmqttconv/converterplugin.hpp"

TEST_CASE ("An instance of map converter") {
    std::string stdconv_path = "../stdconv/stdconv.so";

    boost::shared_ptr<ConverterPlugin> plugin = boost_dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );

    std::shared_ptr<DataConverter> conv(plugin->getConverter("map"));

    SECTION("with int value") {
        std::vector<std::string> args = {
            "{3: 38400}"
        };
        conv->setArgs(args);

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
        std::vector<std::string> args = {
            "{1: \"one\"}"
        };
        conv->setArgs(args);

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
        std::vector<std::string> args = {
            "{ 1 : \" \\\\one \"  }"
        };
        conv->setArgs(args);

        SECTION("should convert register value to mapped string") {
            ModbusRegisters data(1);
            MqttValue ret = conv->toMqtt(data);

            REQUIRE(ret.getString() == " \\one ");
        }
    }

    SECTION("with colon in value") {
        std::vector<std::string> args = {
            "{1:\":\"}"
        };
        conv->setArgs(args);

        SECTION("should convert register value to mapped string") {
            ModbusRegisters data(1);
            MqttValue ret = conv->toMqtt(data);

            REQUIRE(ret.getString() == ":");
        }
    }

    SECTION("with multiple mappings") {
        std::vector<std::string> args = {
            "{1:11,2:\"two\"}"
        };
        conv->setArgs(args);

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

    SECTION("register in hex format") {
        std::vector<std::string> args = {
            "{0x11:17}"
        };
        conv->setArgs(args);

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

    SECTION("string map with two keys") {
        std::vector<std::string> args = {
            "{24:\"t\", 1:\"o\"}"
        };
        conv->setArgs(args);

        SECTION("should convert register value to mapped string") {
            ModbusRegisters data(24);
            MqttValue ret = conv->toMqtt(data);

            REQUIRE(ret.getString() == "t");
        }
    }

    SECTION("string map without braces") {
        std::vector<std::string> args = {
            "24:\"t\", 1:\"o\""
        };
        conv->setArgs(args);

        SECTION("should convert register value to mapped string") {
            ModbusRegisters data(24);
            MqttValue ret = conv->toMqtt(data);

            REQUIRE(ret.getString() == "t");
        }
    }

    SECTION("int map without braces") {
        std::vector<std::string> args = {
            "24:1, 1:2"
        };
        conv->setArgs(args);

        SECTION("should convert register value to mapped string") {
            ModbusRegisters data(24);
            MqttValue ret = conv->toMqtt(data);

            REQUIRE(ret.getString() == "1");
        }
    }

    SECTION("string map with int values") {
        std::vector<std::string> args = {
            "24:\"1\",1:\"2\""
        };
        conv->setArgs(args);

        SECTION("should convert register value to mapped string") {
            ModbusRegisters data(24);
            MqttValue ret = conv->toMqtt(data);

            REQUIRE(ret.getString() == "1");
        }
    }


}
