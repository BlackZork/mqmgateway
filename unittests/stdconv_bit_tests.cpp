#include <libmodmqttsrv/config.hpp>
#include "catch2/catch_all.hpp"
#include "libmodmqttsrv/dll_import.hpp"

#include "libmodmqttconv/converterplugin.hpp"
#include "libmodmqttconv/convexception.hpp"
#include "plugin_utils.hpp"


TEST_CASE("Bit converter") {
    PluginLoader loader("../stdconv/stdconv.so");

    std::shared_ptr<DataConverter> conv(loader.getConverter("bit"));

    SECTION("should return 1 if bit is set") {
        ModbusRegisters data;
        data.appendValue(0x80);

        ConverterArgValues args(conv->getArgs());
        args.setArgValue("bit", "0x8");

        conv->setArgValues(args);
        MqttValue ret = conv->toMqtt(data);

        REQUIRE(ret.getString() == "1");
    }

    SECTION("should return 0 if bit is not set") {
        ModbusRegisters data;
        data.appendValue(0x7FFF);

        ConverterArgValues args(conv->getArgs());
        args.setArgValue("bit", "16");

        conv->setArgValues(args);
        MqttValue ret = conv->toMqtt(data);

        REQUIRE(ret.getString() == "0");
    }

    SECTION("should raise ConvException if arg is out of range") {
        ModbusRegisters data;
        data.appendValue(0x7FFF);

        ConverterArgValues args(conv->getArgs());
        args.setArgValue("bit", "20");

        REQUIRE_THROWS_AS(
            conv->setArgValues(args),
            ConvException
        );
    }

}
