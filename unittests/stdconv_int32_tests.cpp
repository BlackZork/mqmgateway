#include <libmodmqttsrv/config.hpp>
#include <catch2/catch_all.hpp>
#include "libmodmqttsrv/dll_import.hpp"

#include "libmodmqttconv/converterplugin.hpp"
#include "libmodmqttconv/convexception.hpp"

#include "defaults.hpp"

TEST_CASE("int32 tests") {
    std::string stdconv_path = "../stdconv/stdconv.so";

    std::shared_ptr<ConverterPlugin> plugin = modmqttd::boost_dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );

    std::shared_ptr<DataConverter> conv(plugin->getConverter("int32"));

    SECTION("read int32 from two modbus registers (high, low)") {

        ModbusRegisters input({TestNumbers::AB,TestNumbers::CD});
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt() == TestNumbers::ABCD_as_int32);   }

    SECTION("read int32 from two modbus registers (low, high)") {
        std::vector<std::string> args = {
            "low_first"
        };
        conv->setArgs(args);

        ModbusRegisters input({TestNumbers::AB,TestNumbers::CD});
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt() == TestNumbers::CDAB_as_int32);
    }

    // TODO we should output warning in this case, this looks like configuration error
    SECTION("read int32 from a single modbus register ignoring low_first arg") {
        std::vector<std::string> args = {
            "low_first"
        };
        conv->setArgs(args);

        ModbusRegisters input({TestNumbers::AB});
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt() == TestNumbers::AB);
    }


    SECTION("write int32 to two modbus registers (low, high)") {
        std::vector<std::string> args = {
            "low_first"
        };
        conv->setArgs(args);

        MqttValue input(TestNumbers::ABCD_as_int32);
        ModbusRegisters output = conv->toModbus(input, 2);

        REQUIRE(output.getValue(0) == TestNumbers::CD);
        REQUIRE(output.getValue(1) == TestNumbers::AB);
    }

    SECTION("write int32 to two modbus registers (high, low)") {
        MqttValue input(TestNumbers::ABCD_as_int32);
        ModbusRegisters output = conv->toModbus(input, 2);

        REQUIRE(output.getValue(0) == TestNumbers::AB);
        REQUIRE(output.getValue(1) == TestNumbers::CD);
    }
}


