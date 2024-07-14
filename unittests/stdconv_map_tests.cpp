#include <libmodmqttsrv/config.hpp>
#include "catch2/catch_all.hpp"
#include <boost/dll/import.hpp>

#include "libmodmqttconv/converterplugin.hpp"

TEST_CASE ("A map converter") {
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
}
