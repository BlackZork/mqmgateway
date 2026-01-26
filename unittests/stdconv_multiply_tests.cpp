#include <libmodmqttsrv/config.hpp>
#include "catch2/catch_all.hpp"
#include "libmodmqttsrv/dll_import.hpp"

#include "libmodmqttconv/converterplugin.hpp"
#include "plugin_utils.hpp"

TEST_CASE("A one uint16 register value should be multipled") {
    PluginLoader loader("../stdconv/stdconv.so");

    std::shared_ptr<DataConverter> conv(loader.getConverter("multiply"));
    ConverterArgValues args(conv->getArgs());

    args.setArgValue("multipler", "2");

    SECTION("without fractional part if precision is undefined") {
        ModbusRegisters input = ModbusRegisters(std::vector<uint16_t>{10});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getString() == "20");
    }

    SECTION("with fractional part if precision is set") {
        ModbusRegisters input = ModbusRegisters(std::vector<uint16_t>{10});

        args.setArgValue(ConverterArg::sPrecisionArgName, "2");

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getString() == "20.00");
    }
}
