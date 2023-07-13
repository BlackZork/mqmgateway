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

    SECTION("with encoding 'none'") {
        std::shared_ptr<DataConverter> conv(plugin->getConverter("string"));

        SECTION("and with even character count") {
            const char chars[] = "ALD1";
            std::vector<uint16_t> registers({0x4C41, 0x3144});

            SECTION("is read, then its bytes match the register bytes in little-endian order") {
                ModbusRegisters input(registers);
                MqttValue output = conv->toMqtt(input);

                REQUIRE(strcmp(output.getString().data(), chars) == 0);
            }

            SECTION("is written, then its bytes match the register bytes in little-endian order") {
                MqttValue input = MqttValue::fromBinary(chars, sizeof(chars));
                ModbusRegisters output = conv->toModbus(input, 2);

                REQUIRE(output.values() == registers);
            }

            SECTION("is written partially, then the register bytes contain exactly its front part in little-endian order") {
                MqttValue input = MqttValue::fromBinary(chars, sizeof(chars));
                ModbusRegisters output = conv->toModbus(input, 1);

                REQUIRE(output.values() == std::vector<uint16_t>({registers.front()}));
            }
        }

        SECTION("and with odd character count") {
            const char chars[] = "ALD15";
            std::vector<uint16_t> registers({0x4C41, 0x3144, 0x0035});

            SECTION("is read, then its bytes match the register bytes in little-endian order") {
                ModbusRegisters input(registers);
                MqttValue output = conv->toMqtt(input);

                REQUIRE(strcmp(output.getString().data(), chars) == 0);
            }

            SECTION("is written, then its bytes match the register bytes in little-endian order") {
                MqttValue input = MqttValue::fromBinary(chars, sizeof(chars));
                ModbusRegisters output = conv->toModbus(input, 3);

                REQUIRE(output.values() == registers);
            }

            SECTION("is written partially, then the register bytes contain exactly its front part in little-endian order") {
                MqttValue input = MqttValue::fromBinary(chars, sizeof(chars));
                ModbusRegisters output = conv->toModbus(input, 1);

                REQUIRE(output.values() == std::vector<uint16_t>({registers.front()}));
            }
        }
    }

    SECTION("with encoding 'ascii16-be'") {
        std::shared_ptr<DataConverter> conv(plugin->getConverter("string"));
        std::vector<std::string> args = {
            "ascii16-be"
        };
        conv->setArgs(args);

        SECTION("and with even character count") {
            const char chars[] = "ALD1";
            std::vector<uint16_t> registers({0x414C, 0x4431});

            SECTION("is read, then its bytes match the register bytes in big-endian order") {
                ModbusRegisters input(registers);
                MqttValue output = conv->toMqtt(input);

                REQUIRE(strcmp(output.getString().data(), chars) == 0);
            }

            SECTION("is written, then its bytes match the register bytes in big-endian order") {
                MqttValue input = MqttValue::fromBinary(chars, sizeof(chars));
                ModbusRegisters output = conv->toModbus(input, 2);

                REQUIRE(output.values() == registers);
            }

            SECTION("is written partially, then the register bytes contain exactly its front part in big-endian order") {
                MqttValue input = MqttValue::fromBinary(chars, sizeof(chars));
                ModbusRegisters output = conv->toModbus(input, 1);

                REQUIRE(output.values() == std::vector<uint16_t>({registers.front()}));
            }
        }

        SECTION("and with odd character count") {
            const char chars[] = "ALD15";
            std::vector<uint16_t> registers({0x414C, 0x4431, 0x3500});

            SECTION("is read, then its bytes match the register bytes in big-endian order") {
                ModbusRegisters input(registers);
                MqttValue output = conv->toMqtt(input);

                REQUIRE(strcmp(output.getString().data(), chars) == 0);
            }

            SECTION("is written, then its bytes match the register bytes in big-endian order") {
                MqttValue input = MqttValue::fromBinary(chars, sizeof(chars));
                ModbusRegisters output = conv->toModbus(input, 3);

                REQUIRE(output.values() == registers);
            }

            SECTION("is written partially, then the register bytes contain exactly its front part in big-endian order") {
                MqttValue input = MqttValue::fromBinary(chars, sizeof(chars));
                ModbusRegisters output = conv->toModbus(input, 1);

                REQUIRE(output.values() == std::vector<uint16_t>({registers.front()}));
            }
        }
    }

    SECTION("with unsupported encoding is configured, then an exception is thrown") {
        std::shared_ptr<DataConverter> conv(plugin->getConverter("string"));
        std::vector<std::string> args = {
            "utf16-le"
        };

        REQUIRE_THROWS_MATCHES(conv->setArgs(args), ConvException,
                               Catch::Message("Unsupported encoding: utf16-le"));
    }
}
