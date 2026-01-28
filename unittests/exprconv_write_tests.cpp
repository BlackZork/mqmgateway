#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>

#include "libmodmqttconv/converterplugin.hpp"
#include "libmodmqttsrv/config.hpp"

#include "testnumbers.hpp"
#include "plugin_utils.hpp"

#ifdef HAVE_EXPRTK

TEST_CASE ("Expr.evaluate should") {
    PluginLoader loader("../exprconv/exprconv.so");

    std::shared_ptr<DataConverter> conv(loader.getConverter("evaluate"));
    ConverterArgValues args(conv->getArgs());

    SECTION("convert an int value to a single modbus register") {
        args.setArgValue("expression", "M*2");

        MqttValue input(12);

        conv->setArgValues(args);
        ModbusRegisters output = conv->toModbus(input, 1);

        REQUIRE(output.getCount() == 1);
        REQUIRE(output.getValue(0) == 24);
    }

    SECTION("convert a single element table to a single modbus register") {
        args.setArgValue("expression", "return [M*2]");

        MqttValue input(12);

        conv->setArgValues(args);
        ModbusRegisters output = conv->toModbus(input, 1);

        REQUIRE(output.getCount() == 1);
        REQUIRE(output.getValue(0) == 24);
    }

    SECTION("throw if returned value is a string") {
        args.setArgValue("expression", "return ['str']");

        MqttValue input(12);

        conv->setArgValues(args);

        REQUIRE_THROWS_AS(
            conv->toModbus(input, 1),
            ConvException
        );
    }

    SECTION("throw evaluated value is a string") {
        args.setArgValue("expression", "'str'");

        MqttValue input(12);

        conv->setArgValues(args);

        REQUIRE_THROWS_AS(
            conv->toModbus(input, 1),
            ConvException
        );
    }
}

#endif
