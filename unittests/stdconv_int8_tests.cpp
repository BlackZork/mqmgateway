#include "catch2/catch_all.hpp"

#include "libmodmqttconv/converterplugin.hpp"
#include "libmodmqttsrv/config.hpp"
#include "libmodmqttsrv/dll_import.hpp"
#include "plugin_utils.hpp"

TEST_CASE("When reading int8 byte from single register") {
    PluginLoader loader("../stdconv/stdconv.so");

    std::shared_ptr<DataConverter> conv(loader.getConverter("int8"));
    ConverterArgValues args(conv->getArgs());


    ModbusRegisters input(0xff01);

    SECTION("second byte should output correct value") {

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt() == 1);
    }

    SECTION("first byte should output correct value") {
        args.setArgValue("first", "true");

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt() == -1);
    }
}

TEST_CASE("When reading uint8 byte from single register") {
    PluginLoader loader("../stdconv/stdconv.so");

    std::shared_ptr<DataConverter> conv(loader.getConverter("uint8"));
    ConverterArgValues args(conv->getArgs());

    ModbusRegisters input(0xff01);

    SECTION("second byte should output correct value") {

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt() == 1);
    }

    SECTION("first byte should output correct value") {
        args.setArgValue("first", "true");

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt() == 255);
    }
}
