#include <libmodmqttsrv/config.hpp>
#include "catch2/catch_all.hpp"
#include <boost/dll/import.hpp>

#include "libmodmqttconv/converterplugin.hpp"


TEST_CASE("When reading int8 byte from single register") {
    std::string stdconv_path = "../stdconv/stdconv.so";

    boost::shared_ptr<ConverterPlugin> plugin = boost_dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );

    std::shared_ptr<DataConverter> conv(plugin->getConverter("int8"));
    ModbusRegisters input(0xff01);

    SECTION("second byte should output correct value") {

        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt() == 1);
    }

    SECTION("first byte should output correct value") {
        std::vector<std::string> args = {
            "first"
        };
        conv->setArgs(args);

        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt() == -1);
    }
}

TEST_CASE("When reading uint8 byte from single register") {
    std::string stdconv_path = "../stdconv/stdconv.so";

    boost::shared_ptr<ConverterPlugin> plugin = boost_dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );

    std::shared_ptr<DataConverter> conv(plugin->getConverter("uint8"));
    ModbusRegisters input(0xff01);

    SECTION("second byte should output correct value") {

        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt() == 1);
    }

    SECTION("first byte should output correct value") {
        std::vector<std::string> args = {
            "first"
        };
        conv->setArgs(args);

        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt() == 255);
    }
}
