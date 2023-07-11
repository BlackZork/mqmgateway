#include <libmodmqttsrv/config.hpp>
#include "catch2/catch.hpp"
#include <boost/dll/import.hpp>

#include "libmodmqttconv/converterplugin.hpp"

TEST_CASE("A number value should be divided") {
    std::string stdconv_path = "../stdconv/stdconv.so";

    boost::shared_ptr<ConverterPlugin> plugin = boost_dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );
    std::shared_ptr<DataConverter> conv(plugin->getConverter("divide"));
    std::vector<std::string> args = {
        "2"
    };

    SECTION("when one int32 value is converted to two modbus registers, high byte first") {
        MqttValue input(0x20004);

        conv->setArgs(args);
        ModbusRegisters output = conv->toModbus(input, 2);

        REQUIRE(output.values() == std::vector<uint16_t>({0x1, 0x2}));
    }


    SECTION("when one int32 value is converted to two modbus registers, low byte first") {
        MqttValue input(0x20004);

        args.push_back("1");
        args.push_back("low_first");
        conv->setArgs(args);

        ModbusRegisters output = conv->toModbus(input, 2);

        REQUIRE(output.values() == std::vector<uint16_t>({0x2, 0x1}));
    }


    SECTION("when two modbus registers are converted into a string, high byte first") {
        ModbusRegisters input = ModbusRegisters(std::vector<uint16_t>{0x01, 0x2});

        conv->setArgs(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getString() == "32769");
    }


    SECTION("when two modbus registers are converted into a string, low byte first") {
        ModbusRegisters input = ModbusRegisters(std::vector<uint16_t>{0x02, 0x1});

        args.push_back("1");
        args.push_back("low_first");
        conv->setArgs(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getString() == "32769");
    }


    SECTION("when single modbus register is converted into a string") {
        ModbusRegisters input = ModbusRegisters(std::vector<uint16_t>{0x02});

        conv->setArgs(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getString() == "1");
    }


    SECTION("mqtt value is converted to single modbus register") {
        MqttValue input(0x4);

        conv->setArgs(args);
        ModbusRegisters output = conv->toModbus(input, 1);

        REQUIRE(output.values() == std::vector<uint16_t>({0x2}));
    }
}


TEST_CASE("When precision of divided mqtt value ") {
    std::string stdconv_path = "../stdconv/stdconv.so";

    boost::shared_ptr<ConverterPlugin> plugin = boost_dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );
    std::shared_ptr<DataConverter> conv(plugin->getConverter("divide"));
    std::vector<std::string> args = {
        "3"
    };

    SECTION("is default 6 then fractional part contains 6 digits") {
        ModbusRegisters input = ModbusRegisters(std::vector<uint16_t>{10});

        conv->setArgs(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getString() == "3.333333");
    }

    SECTION("is 3 then fractional part contains 3 digits") {
        ModbusRegisters input = ModbusRegisters(std::vector<uint16_t>{10});

        args.push_back("3");
        conv->setArgs(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getString() == "3.333");
    }

    SECTION("is 0 then there is no fractional part") {
        ModbusRegisters input = ModbusRegisters(std::vector<uint16_t>{10});

        args.push_back("0");
        conv->setArgs(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getString() == "3");
    }
}
