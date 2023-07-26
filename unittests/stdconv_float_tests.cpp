#include <libmodmqttsrv/config.hpp>
#include "catch2/catch.hpp"
#include <boost/dll/import.hpp>

#include "libmodmqttconv/converterplugin.hpp"

TEST_CASE("A float32 value should be read") {
    std::string stdconv_path = "../stdconv/stdconv.so";

    boost::shared_ptr<ConverterPlugin> plugin = boost_dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );
    std::shared_ptr<DataConverter> conv(plugin->getConverter("float32"));
    std::vector<std::string> args;

    const float expected = -123.456f; // 0xc2f6e979 in IEEE 754 hex representation
    const std::string expectedString = "-123.456001";

    SECTION("when two registers contains a float ABCD format") {
        const ModbusRegisters input({0xc2f6, 0xe979});

        MqttValue output = conv->toMqtt(input);


        REQUIRE(output.getDouble() == expected);
        REQUIRE(output.getString() == expectedString);
    }

    SECTION("when two registers contains a float CDAB format") {
        const ModbusRegisters input({0xe979, 0xc2f6});

        conv->setArgs(std::vector<std::string>({"-1", "low_first"}));
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getDouble() == expected);
        REQUIRE(output.getString() == expectedString);
    }

    SECTION("when two registers contains a float BADC format") {
        const ModbusRegisters input({0xf6c2, 0x79e9});

        conv->setArgs(std::vector<std::string>({"-1", "high_first", "swap_bytes"}));
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getDouble() == expected);
        REQUIRE(output.getString() == expectedString);
    }


    SECTION("when two registers contains a float DCBA format") {
        const ModbusRegisters input({0x79e9, 0xf6c2});

        conv->setArgs(std::vector<std::string>({"-1", "low_first", "swap_bytes"}));
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getDouble() == expected);
        REQUIRE(output.getString() == expectedString);
    }

    SECTION("when precision is set") {
        const ModbusRegisters input({0xc2f6, 0xe979});

        conv->setArgs(std::vector<std::string>({"2"}));
        MqttValue output = conv->toMqtt(input);


        REQUIRE(output.getString() == "-123.45");
    }

}

TEST_CASE("A float32 value should be written") {
    std::string stdconv_path = "../stdconv/stdconv.so";

    boost::shared_ptr<ConverterPlugin> plugin = boost_dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );
    std::shared_ptr<DataConverter> conv(plugin->getConverter("float32"));
    std::vector<std::string> args;

    std::string val("-123.456001");
    MqttValue input = MqttValue::fromBinary(val.c_str(), val.length());

    SECTION("to two registers in ABCD format") {
        const ModbusRegisters converted = conv->toModbus(input, 2);
        const ModbusRegisters expected({0xc2f6, 0xe979});

        REQUIRE(converted.values() == expected.values());
    }

    SECTION("to two registers in CDAB format") {
        conv->setArgs(std::vector<std::string>({"-1", "low_first"}));
        const ModbusRegisters converted = conv->toModbus(input, 2);
        const ModbusRegisters expected({0xe979, 0xc2f6});

        REQUIRE(converted.values() == expected.values());
    }

    SECTION("to two registers in BADC format") {
        conv->setArgs(std::vector<std::string>({"-1", "high_first", "swap_bytes"}));
        const ModbusRegisters converted = conv->toModbus(input, 2);
        const ModbusRegisters expected({0xf6c2, 0x79e9});

        REQUIRE(converted.values() == expected.values());
    }

    SECTION("to two registers in DCBA format") {
        conv->setArgs(std::vector<std::string>({"-1", "low_first", "swap_bytes"}));
        const ModbusRegisters converted = conv->toModbus(input, 2);
        const ModbusRegisters expected({0x79e9, 0xf6c2});

        REQUIRE(converted.values() == expected.values());
    }


}
