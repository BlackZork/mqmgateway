#include <libmodmqttsrv/config.hpp>
#include <catch2/catch_all.hpp>
#include "libmodmqttsrv/dll_import.hpp"

#include "libmodmqttconv/converterplugin.hpp"
#include "libmodmqttconv/convexception.hpp"

#include "testnumbers.hpp"

TEST_CASE("when std.uint32") {
    std::string stdconv_path = "../stdconv/stdconv.so";

    std::shared_ptr<ConverterPlugin> plugin = modmqttd::boost_dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );

    std::shared_ptr<DataConverter> conv(plugin->getConverter("int32"));

    SECTION("converts two modbus registers (high, low)") {

        ModbusRegisters input({TestNumbers::Int::AB,TestNumbers::Int::CD});
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt() == TestNumbers::Int::ABCD_as_uint32);   }

    SECTION("converts two modbus registers (low, high)") {
        std::vector<std::string> args = {
            "low_first"
        };
        conv->setArgs(args);

        ModbusRegisters input({TestNumbers::Int::AB,TestNumbers::Int::CD});
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt() == TestNumbers::Int::CDAB_as_uint32);
    }

    SECTION("writes to two modbus registers (low, high)") {
        std::vector<std::string> args = {
            "low_first"
        };
        conv->setArgs(args);

        MqttValue input = MqttValue::fromInt64(TestNumbers::Int::ABCD_as_uint32);
        ModbusRegisters output = conv->toModbus(input, 2);

        REQUIRE(output.getValue(0) == TestNumbers::Int::CD);
        REQUIRE(output.getValue(1) == TestNumbers::Int::AB);
    }

    SECTION("writes to two modbus registers (high, low)") {
        MqttValue input = MqttValue::fromInt64(TestNumbers::Int::ABCD_as_uint32);
        ModbusRegisters output = conv->toModbus(input, 2);

        REQUIRE(output.getValue(0) == TestNumbers::Int::AB);
        REQUIRE(output.getValue(1) == TestNumbers::Int::CD);
    }
}


