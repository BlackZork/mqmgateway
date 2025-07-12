#include <catch2/catch_all.hpp>
#include "libmodmqttconv/converterplugin.hpp"
#include "libmodmqttsrv/config.hpp"
#include "libmodmqttsrv/dll_import.hpp"

#ifdef HAVE_EXPRTK

TEST_CASE ("exprtk should read int32 from two registers") {
    std::string stdconv_path = "../exprconv/exprconv.so";
    std::shared_ptr<ConverterPlugin> plugin = modmqttd::boost_dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );
    std::shared_ptr<DataConverter> conv(plugin->getConverter("evaluate"));

    SECTION("when two registers contain a signed integer") {
        conv->setArgs({"int32(R0, R1)"});
        const ModbusRegisters input({0xdcfe, 0x98ba});
        const int32_t expected = 0xfedcba98;

        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getDouble() == expected);
        REQUIRE(output.getString() == "-19088744");
    }

    SECTION("when two registers contain an unsigned integer") {
        conv->setArgs({"uint32(R0, R1)"});
        const ModbusRegisters input({0xdcfe, 0x98ba});

        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getDouble() == 0xfedcba98);
        REQUIRE(output.getString() == "4275878552");
    }
}

#endif
