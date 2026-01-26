#include <catch2/catch_all.hpp>
#include "libmodmqttconv/converterplugin.hpp"
#include "libmodmqttsrv/config.hpp"
#include "libmodmqttsrv/dll_import.hpp"

#ifdef HAVE_EXPRTK

TEST_CASE ("A number should be converted by exprtk") {
    std::string stdconv_path = "../exprconv/exprconv.so";
    std::shared_ptr<ConverterPlugin> plugin = modmqttd::dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin"
    );
    std::shared_ptr<DataConverter> conv(plugin->getConverter("evaluate"));
    ConverterArgValues args(conv->getArgs());

    SECTION("when precision is not set") {
        args.setArgValue("expression", ConverterArgType::STRING, "R0 * 2");
        const ModbusRegisters input(10);

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getString() == "20");
    }

    SECTION("when precision is set") {
        args.setArgValue("expression", ConverterArgType::STRING, "R0 / 3");
        args.setArgValue(ConverterArg::sPrecisionArgName, ConverterArgType::INT, "3");
        const ModbusRegisters input(10);

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getString() == "3.333");
    }
}

TEST_CASE ("A uint16_t register data should be converted to exprtk value") {
    std::string stdconv_path = "../exprconv/exprconv.so";
    std::shared_ptr<ConverterPlugin> plugin = modmqttd::dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin"
    );
    std::shared_ptr<DataConverter> conv(plugin->getConverter("evaluate"));
    ConverterArgValues args(conv->getArgs());

    args.setArgValue("expression", ConverterArgType::STRING, "int16(R0)");
    const ModbusRegisters input(0xFFFF);

    conv->setArgValues(args);
    MqttValue output = conv->toMqtt(input);

    REQUIRE(output.getString() == "-1");
}

#endif
