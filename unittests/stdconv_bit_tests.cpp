#include <libmodmqttsrv/config.hpp>
#include "catch2/catch_all.hpp"
#include "libmodmqttsrv/dll_import.hpp"

#include "libmodmqttconv/converterplugin.hpp"
#include "libmodmqttconv/convexception.hpp"


TEST_CASE("Bit converter") {
    std::string stdconv_path = "../stdconv/stdconv.so";

    std::shared_ptr<ConverterPlugin> plugin = modmqttd::boost_dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );

    std::shared_ptr<DataConverter> conv(plugin->getConverter("bit"));

    SECTION("should return 1 if bit is set") {
        ModbusRegisters data;
        data.appendValue(0x80);

        conv->setArgs(std::vector<std::string>({"8"}));
        MqttValue ret = conv->toMqtt(data);

        REQUIRE(ret.getString() == "1");
    }

    SECTION("should return 0 if bit is not set") {
        ModbusRegisters data;
        data.appendValue(0x7FFF);

        conv->setArgs(std::vector<std::string>({"16"}));
        MqttValue ret = conv->toMqtt(data);

        REQUIRE(ret.getString() == "0");
    }

    SECTION("should raise ConversionException if arg is out of range") {
        ModbusRegisters data;
        data.appendValue(0x7FFF);

        REQUIRE_THROWS_AS(
            conv->setArgs(std::vector<std::string>({"20"})),
            ConvException
        );
    }

}
