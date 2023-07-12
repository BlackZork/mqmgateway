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

    std::shared_ptr<DataConverter> conv(plugin->getConverter("scale"));
    std::vector<std::string> args = {
        "4","20",
        "0","400"
    };
    conv->setArgs(args);

    ModbusRegisters data;
    data.appendValue(8);
    MqttValue ret = conv->toMqtt(data);

    REQUIRE(ret.getString() == "100");
}

TEST_CASE("int32 tests") {
    std::string stdconv_path = "../stdconv/stdconv.so";

    boost::shared_ptr<ConverterPlugin> plugin = boost_dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );

    std::shared_ptr<DataConverter> conv(plugin->getConverter("int32"));

    SECTION("read int32 from two modbus registers (high, low)") {

        ModbusRegisters input({32768,1});
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt() == -2147483647);
    }

    SECTION("read int32 from two modbus registers (low, high)") {
        std::vector<std::string> args = {
            "low_first"
        };
        conv->setArgs(args);

        ModbusRegisters input({1, 32768});
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt() == -2147483647);
    }

    SECTION("write int32 to two modbus registers (low, high)") {
        std::vector<std::string> args = {
            "low_first"
        };
        conv->setArgs(args);

        MqttValue input(0x20004);
        ModbusRegisters output = conv->toModbus(input, 2);

        REQUIRE(output.getValue(0) == 0x4);
        REQUIRE(output.getValue(1) == 0x2);
    }

    SECTION("write int32 to two modbus registers (high, low)") {
        MqttValue input(0x20004);
        ModbusRegisters output = conv->toModbus(input, 2);

        REQUIRE(output.getValue(0) == 0x2);
        REQUIRE(output.getValue(1) == 0x4);
    }
}

TEST_CASE ("read int16 value") {
    std::string stdconv_path = "../stdconv/stdconv.so";

    boost::shared_ptr<ConverterPlugin> plugin = boost_dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );

    std::shared_ptr<DataConverter> conv(plugin->getConverter("int16"));

    ModbusRegisters data;
    data.appendValue(0xFFFF);
    MqttValue ret = conv->toMqtt(data);

    REQUIRE(ret.getString() == "-1");
}


TEST_CASE("uint32 tests") {
    std::string stdconv_path = "../stdconv/stdconv.so";

    boost::shared_ptr<ConverterPlugin> plugin = boost_dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );

    std::shared_ptr<DataConverter> conv(plugin->getConverter("uint32"));

    SECTION("read uint32 value") {
        ModbusRegisters data;
        data.appendValue(0x9234);
        data.appendValue(0xABCD);
        MqttValue ret = conv->toMqtt(data);

        REQUIRE(ret.getString() == "2452925389");
    }


    SECTION("write uint32 value") {
        MqttValue input(2452925389);
        ModbusRegisters output = conv->toModbus(input, 2);

        REQUIRE(output.getValue(0) == 0x9234);
        REQUIRE(output.getValue(1) == 0xABCD);
    }

}

TEST_CASE("Values of type string should be converted") {
    std::string stdconv_path = "../stdconv/stdconv.so";

    boost::shared_ptr<ConverterPlugin> plugin = boost_dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );

    SECTION("when byte-order is (high, low)") {
        std::shared_ptr<DataConverter> conv(plugin->getConverter("string"));
        std::vector<std::string> args = {
            "ascii-be"
        };
        conv->setArgs(args);
        const char chars[] = "ALD15";
        std::vector<uint16_t> registers({0x414C, 0x4431, 0x3500});

        SECTION("and string value is read") {
            ModbusRegisters input(registers);
            MqttValue output = conv->toMqtt(input);

            REQUIRE(strcmp(output.getString().data(), chars) == 0);
        }

        SECTION("and string value is written") {
            MqttValue input = MqttValue::fromBinary(chars, sizeof(chars));
            ModbusRegisters output = conv->toModbus(input, 3);

            REQUIRE(output.values() == registers);
        }

        SECTION("and string value is written partially") {
            MqttValue input = MqttValue::fromBinary(chars, sizeof(chars));
            ModbusRegisters output = conv->toModbus(input, 1);

            REQUIRE(output.values() == std::vector<uint16_t>({registers.front()}));
        }
    }

    SECTION("when byte-order is (low, high)") {
        std::shared_ptr<DataConverter> conv(plugin->getConverter("string"));
        const char chars[] = "ALD15";
        std::vector<uint16_t> registers({0x4C41, 0x3144, 0x0035});

        SECTION("and string value is read") {
            ModbusRegisters input(registers);
            MqttValue output = conv->toMqtt(input);

            REQUIRE(strcmp(output.getString().data(), chars) == 0);
        }

        SECTION("and string value is written") {
            MqttValue input = MqttValue::fromBinary(chars, sizeof(chars));
            ModbusRegisters output = conv->toModbus(input, 3);

            REQUIRE(output.values() == registers);
        }

        SECTION("and string value is written partially") {
            MqttValue input = MqttValue::fromBinary(chars, sizeof(chars));
            ModbusRegisters output = conv->toModbus(input, 1);

            REQUIRE(output.values() == std::vector<uint16_t>({registers.front()}));
        }
    }
}
