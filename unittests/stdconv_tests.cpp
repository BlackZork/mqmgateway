#include <libmodmqttsrv/config.hpp>
#include "catch2/catch_all.hpp"
#include "libmodmqttsrv/dll_import.hpp"

#include "libmodmqttconv/converterplugin.hpp"
#include "libmodmqttconv/convexception.hpp"

TEST_CASE ("Scale value with integer result") {
    std::string stdconv_path = "../stdconv/stdconv.so";

    std::shared_ptr<ConverterPlugin> plugin = modmqttd::boost_dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );

    std::shared_ptr<DataConverter> conv(plugin->getConverter("scale"));
    std::vector<std::string> args = {
        "4","20",
        "0","400"
    };
    conv->setArgs(args);

    ModbusRegisters data;
    data.appendValue(8);
    MqttValue ret = conv->toMqtt(data);

    REQUIRE(ret.getString() == "100");
}

TEST_CASE ("read int16 value") {
    std::string stdconv_path = "../stdconv/stdconv.so";

    std::shared_ptr<ConverterPlugin> plugin = modmqttd::boost_dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );

    std::shared_ptr<DataConverter> conv(plugin->getConverter("int16"));

    ModbusRegisters data;
    data.appendValue(0xFFFF);
    MqttValue ret = conv->toMqtt(data);

    REQUIRE(ret.getString() == "-1");
}


TEST_CASE("uint32 tests") {
    std::string stdconv_path = "../stdconv/stdconv.so";

    std::shared_ptr<ConverterPlugin> plugin = modmqttd::boost_dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );

    std::shared_ptr<DataConverter> conv(plugin->getConverter("uint32"));

    SECTION("read uint32 value") {
        ModbusRegisters data;
        data.appendValue(0x9234);
        data.appendValue(0xABCD);
        MqttValue ret = conv->toMqtt(data);

        REQUIRE(ret.getString() == "2452925389");
    }


    SECTION("write uint32 value") {
        MqttValue input(2452925389);
        ModbusRegisters output = conv->toModbus(input, 2);

        REQUIRE(output.getValue(0) == 0x9234);
        REQUIRE(output.getValue(1) == 0xABCD);
    }

}

