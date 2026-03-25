#include <catch2/catch_all.hpp>
#include "libmodmqttconv/converterplugin.hpp"
#include "libmodmqttsrv/config.hpp"
#include "libmodmqttsrv/dll_import.hpp"
#include "plugin_utils.hpp"

#ifdef HAVE_EXPRTK

TEST_CASE ("A number should be converted by exprtk") {
    PluginLoader loader("../exprconv/exprconv.so");

    std::shared_ptr<DataConverter> conv(loader.getConverter("evaluate"));
    ConverterArgValues args(conv->getArgs());

    SECTION("when precision is not set") {
        args.setArgValue("expression", "R0 * 2");
        const ModbusRegisters input(10);

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getString() == "20");
    }

    SECTION("when precision is set") {
        args.setArgValue("expression", "R0 / 3");
        args.setArgValue(ConverterArg::sPrecisionArgName, "3");
        const ModbusRegisters input(10);

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getString() == "3.333");
    }
}

#endif
