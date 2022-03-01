#include <libmodmqttsrv/config.hpp>
#include "catch2/catch.hpp"
#include <boost/dll/import.hpp>

#include "libmodmqttconv/converterplugin.hpp"

TEST_CASE ("Scale value with integer result") {
    std::string stdconv_path = "../stdconv/stdconv.so";

    boost::shared_ptr<ConverterPlugin> plugin = boost_dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );

    std::shared_ptr<IStateConverter> conv(plugin->getStateConverter("scale"));
    std::vector<std::string> args = {
        "4","20",
        "0","400"
    };
    conv->setArgs(args);

    ModbusRegisters data;
    data.addValue(8);
    MqttValue ret = conv->toMqtt(data);

    REQUIRE(ret.getString() == "100");
}
