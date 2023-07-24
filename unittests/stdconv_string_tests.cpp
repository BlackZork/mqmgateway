#include <boost/dll/import.hpp>
#include <catch2/catch.hpp>
#include "libmodmqttconv/converterplugin.hpp"
#include "libmodmqttsrv/config.hpp"

TEST_CASE("When a string") {
    std::string stdconv_path = "../stdconv/stdconv.so";
    boost::shared_ptr<ConverterPlugin> plugin = boost_dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );
    std::shared_ptr<DataConverter> conv(plugin->getConverter("string"));

    const char charsEven[] = "ABCD";
    const char charsOdd[]  = "ABC";
    const char charsWithNullBytes[] = "AB\u0000C\u0000D";
    const std::vector<uint16_t> registersEven({0x4142, 0x4344});
    const std::vector<uint16_t> registersOdd({0x4142, 0x4300});
    const std::vector<uint16_t> registersWithNullBytes({0x4142, 0x0043, 0x0044});

    SECTION("is read from registers") {
        SECTION("and the character count is even, then it contains exactly the characters from the registers") {
            ModbusRegisters input(registersEven);
            MqttValue output = conv->toMqtt(input);

            REQUIRE(output.getString() == charsEven);
            REQUIRE(output.getBinarySize() == strlen(charsEven));
        }

        SECTION("and the character count is odd, then it contains all characters from the registers except the trailing null byte") {
            ModbusRegisters input(registersOdd);
            MqttValue output = conv->toMqtt(input);

            REQUIRE(output.getString() == charsOdd);
            REQUIRE(output.getBinarySize() == strlen(charsOdd));
        }

        SECTION("and the registers contain null bytes, then it is truncated at the first null byte") {
            ModbusRegisters input(registersWithNullBytes);
            const char expected[] = "AB";
            MqttValue output = conv->toMqtt(input);

            REQUIRE(output.getString() == expected);
            REQUIRE(output.getBinarySize() == strlen(expected));
        }
    }

    SECTION("is written to registers") {
        SECTION("and the character count is even, then the registers contain exactly the characters from the string") {
            MqttValue input = MqttValue::fromBinary(charsEven, sizeof(charsEven));
            ModbusRegisters output = conv->toModbus(input, registersEven.size());

            REQUIRE(output.values() == registersEven);
        }

        SECTION("and the character count is odd, then the registers contain the characters from the string and a trailing null byte") {
            MqttValue input = MqttValue::fromBinary(charsOdd, sizeof(charsOdd));
            ModbusRegisters output = conv->toModbus(input, registersOdd.size());

            REQUIRE(output.values() == registersOdd);
        }

        SECTION("and the character count is shorter than the registers, then the registers are filled with null bytes") {
            MqttValue input = MqttValue::fromBinary(charsOdd, sizeof(charsOdd));
            std::vector<uint16_t> expected = registersOdd;
            expected.push_back(0);
            ModbusRegisters output = conv->toModbus(input, expected.size());

            REQUIRE(output.values() == expected);
        }

        SECTION("and the character count is longer than the registers, then the registers contain the truncated string") {
            MqttValue input = MqttValue::fromBinary(charsOdd, sizeof(charsOdd));
            std::vector<uint16_t> expected = std::vector<uint16_t>({registersOdd.front()});
            ModbusRegisters output = conv->toModbus(input, expected.size());

            REQUIRE(output.values() == expected);
        }

        SECTION("and contains null bytes, then the registers contain exactly the characters from the string") {
            MqttValue input = MqttValue::fromBinary(charsWithNullBytes, sizeof(charsWithNullBytes));
            ModbusRegisters output = conv->toModbus(input, registersWithNullBytes.size());

            REQUIRE(output.values() == registersWithNullBytes);
        }
    }
}
