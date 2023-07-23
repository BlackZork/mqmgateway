#include <boost/dll/import.hpp>
#include <catch2/catch.hpp>
#include "libmodmqttconv/converterplugin.hpp"
#include "libmodmqttsrv/config.hpp"

TEST_CASE("A string should be converted") {
    std::string stdconv_path = "../stdconv/stdconv.so";
    boost::shared_ptr<ConverterPlugin> plugin = boost_dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );
    std::shared_ptr<DataConverter> conv(plugin->getConverter("string"));

    const char chars[] = "ABC\u0000";
    std::vector<uint16_t> registers({0x4142, 0x4300});

    SECTION("when it is read from registers without converter args") {
        ModbusRegisters input(registers);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(strcmp(output.getString().data(), chars) == 0);
    }

    SECTION("when a substring is read from registers") {
        conv->setArgs({"3"});
        ModbusRegisters input(registers);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getString() == std::string(chars, 3).data());
    }

    SECTION("when it is written to registers") {
        MqttValue input = MqttValue::fromBinary(chars, sizeof(chars));
        ModbusRegisters output = conv->toModbus(input, 2);

        REQUIRE(output.values() == registers);
    }

    SECTION("when a substring is written to registers") {
        MqttValue input = MqttValue::fromBinary(chars, sizeof(chars));
        ModbusRegisters output = conv->toModbus(input, 1);

        REQUIRE(output.values() == std::vector<uint16_t>({registers.front()}));
    }
}
