#include <libmodmqttsrv/config.hpp>
#include "catch2/catch_all.hpp"
#include "libmodmqttsrv/dll_import.hpp"

#include "libmodmqttconv/converterplugin.hpp"
#include "libmodmqttconv/convexception.hpp"

TEST_CASE ("Scale value with integer result") {
    std::string stdconv_path = "../stdconv/stdconv.so";

    std::shared_ptr<ConverterPlugin> plugin = modmqttd::dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin"
    );

    std::shared_ptr<DataConverter> conv(plugin->getConverter("scale"));
    ConverterArgValues args(conv->getArgs());

    args.setArgValue("src_from", ConverterArgType::DOUBLE, "4");
    args.setArgValue("src_to", ConverterArgType::DOUBLE, "20");
    args.setArgValue("tgt_from", ConverterArgType::DOUBLE, "0");
    args.setArgValue("tgt_to", ConverterArgType::DOUBLE, "400");

    ModbusRegisters data;
    data.appendValue(8);

    conv->setArgValues(args);
    MqttValue ret = conv->toMqtt(data);

    REQUIRE(ret.getString() == "100");
}

TEST_CASE ("read int16 value") {
    std::string stdconv_path = "../stdconv/stdconv.so";

    std::shared_ptr<ConverterPlugin> plugin = modmqttd::dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin"
    );

    std::shared_ptr<DataConverter> conv(plugin->getConverter("int16"));
    ConverterArgValues args(conv->getArgs());

    ModbusRegisters data;
    data.appendValue(0xFFFF);

    conv->setArgValues(args);
    MqttValue ret = conv->toMqtt(data);

    REQUIRE(ret.getString() == "-1");
}


