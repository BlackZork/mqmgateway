#include <libmodmqttsrv/config.hpp>
#include "catch2/catch_all.hpp"
#include "libmodmqttsrv/dll_import.hpp"

#include "libmodmqttconv/converterplugin.hpp"
#include "libmodmqttconv/convexception.hpp"
#include "plugin_utils.hpp"

TEST_CASE ("Scale value with integer result") {
    PluginLoader loader("../stdconv/stdconv.so");

    std::shared_ptr<DataConverter> conv(loader.getConverter("scale"));
    ConverterArgValues args(conv->getArgs());

    args.setArgValue("src_from", "4");
    args.setArgValue("src_to", "20");
    args.setArgValue("tgt_from", "0");
    args.setArgValue("tgt_to", "400");

    ModbusRegisters data;
    data.appendValue(8);

    conv->setArgValues(args);
    MqttValue ret = conv->toMqtt(data);

    REQUIRE(ret.getString() == "100");
}

TEST_CASE ("read int16 value") {
    PluginLoader loader("../stdconv/stdconv.so");

    std::shared_ptr<DataConverter> conv(loader.getConverter("int16"));
    ConverterArgValues args(conv->getArgs());

    ModbusRegisters data;
    data.appendValue(0xFFFF);

    conv->setArgValues(args);
    MqttValue ret = conv->toMqtt(data);

    REQUIRE(ret.getString() == "-1");
}


