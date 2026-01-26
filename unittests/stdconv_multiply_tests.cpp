#include <libmodmqttsrv/config.hpp>
#include "catch2/catch_all.hpp"
#include "libmodmqttsrv/dll_import.hpp"

#include "libmodmqttconv/converterplugin.hpp"

TEST_CASE("A one uint16 register value should be multipled") {
    std::string stdconv_path = "../stdconv/stdconv.so";

    std::shared_ptr<ConverterPlugin> plugin = modmqttd::dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin"
    );
    std::shared_ptr<DataConverter> conv(plugin->getConverter("multiply"));
    ConverterArgValues args(conv->getArgs());

    args.setArgValue("multipler", ConverterArgType::DOUBLE, "2");

    SECTION("without fractional part if precision is undefined") {
        ModbusRegisters input = ModbusRegisters(std::vector<uint16_t>{10});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getString() == "20");
    }

    SECTION("with fractional part if precision is set") {
        ModbusRegisters input = ModbusRegisters(std::vector<uint16_t>{10});

        args.setArgValue(ConverterArg::sPrecisionArgName, ConverterArgType::INT, "2");

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getString() == "20.00");
    }
}
