#include "catch2/catch.hpp"
#include <boost/dll/import.hpp>

#include "libmodmqttconv/converterplugin.hpp"

#ifdef HAVE_EXPRTK

TEST_CASE ("Simple expression converter test") {
    std::string stdconv_path = "../exprconv/exprconv.so";

    boost::shared_ptr<ConverterPlugin> plugin = boost::dll::import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );

    std::shared_ptr<IStateConverter> conv(plugin->getStateConverter("expr"));
    std::vector<std::string> args = {
        "R0 * 2"
    };
    conv->setArgs(args);

    ModbusRegisters data;
    data.addValue(10);
    MqttValue ret = conv->toMqtt(data);

    REQUIRE(ret.getString() == "20");
}

#endif
