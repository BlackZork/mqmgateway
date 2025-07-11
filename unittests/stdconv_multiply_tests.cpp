#include <libmodmqttsrv/config.hpp>
#include "catch2/catch_all.hpp"
#include "libmodmqttsrv/dll_import.hpp"

#include "libmodmqttconv/converterplugin.hpp"

TEST_CASE("A one uint16 register value should be multipled") {
    std::string stdconv_path = "../stdconv/stdconv.so";

    std::shared_ptr<ConverterPlugin> plugin = modmqttd::boost_dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );
    std::shared_ptr<DataConverter> conv(plugin->getConverter("multiply"));
    std::vector<std::string> args = {
        "2"
    };

    SECTION(" without fractional part if precision is undefined") {
        ModbusRegisters input = ModbusRegisters(std::vector<uint16_t>{10});

        conv->setArgs(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getString() == "20");
    }

    SECTION(" with fractional part if precision is set") {
        ModbusRegisters input = ModbusRegisters(std::vector<uint16_t>{10});

        args.push_back("2");
        conv->setArgs(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getString() == "20.00");
    }
}
