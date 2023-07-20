#include <boost/dll/import.hpp>
#include <catch2/catch.hpp>
#include "libmodmqttconv/converterplugin.hpp"
#include "libmodmqttsrv/config.hpp"

#ifdef HAVE_EXPRTK

TEST_CASE ("Exprtk converter test") {
    std::string stdconv_path = "../exprconv/exprconv.so";

    boost::shared_ptr<ConverterPlugin> plugin = boost_dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );

    std::shared_ptr<DataConverter> conv(plugin->getConverter("evaluate"));
    std::vector<std::string> args = {
        "R0 * 2"
    };
    conv->setArgs(args);

    ModbusRegisters data;
    data.appendValue(10);
    MqttValue ret = conv->toMqtt(data);

    REQUIRE(ret.getString() == "20");
}

TEST_CASE ("Exprtk converter test with precission") {
    std::string stdconv_path = "../exprconv/exprconv.so";

    boost::shared_ptr<ConverterPlugin> plugin = boost_dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );

    std::shared_ptr<DataConverter> conv(plugin->getConverter("evaluate"));
    std::vector<std::string> args = {
        "R0 / 3", "3"
    };
    conv->setArgs(args);

    ModbusRegisters data;
    data.appendValue(10);
    MqttValue ret = conv->toMqtt(data);

    REQUIRE(ret.getString() == "3.333");
}


TEST_CASE("A 32-bit number should be converted by exprtk") {
    std::string stdconv_path = "../exprconv/exprconv.so";
    boost::shared_ptr<ConverterPlugin> plugin = boost_dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );
    std::shared_ptr<DataConverter> conv(plugin->getConverter("evaluate"));

    SECTION("when two registers contain a float") {
        const float expected = -123.456f; // 0xc2f6e979 in IEEE 754 hex representation

        SECTION("and byte order is big-endian") {
            conv->setArgs({"flt32be(R0, R1)"});
            const ModbusRegisters input({0xc2f6, 0xe979});

            MqttValue output = conv->toMqtt(input);

            REQUIRE(output.getDouble() == expected);
            REQUIRE(output.getString() == "-123.456001");
        }

        SECTION("and byte order is little-endian") {
            conv->setArgs({"flt32le(R0, R1)"});
            const ModbusRegisters input({0xf6c2, 0x79e9});

            MqttValue output = conv->toMqtt(input);

            REQUIRE(output.getDouble() == expected);
            REQUIRE(output.getString() == "-123.456001");
        }

        SECTION("and precision is set") {
            conv->setArgs({"flt32be(R0, R1)", "3"});
            const ModbusRegisters input({0xc2f6, 0xe979});

            MqttValue output = conv->toMqtt(input);

            REQUIRE(output.getString() == "-123.456");
        }
    }

    SECTION("when two registers contain a signed integer") {
        const int32_t expected = 0xfedcba98;
        const std::string expectedString = "-19088744";

        SECTION("and byte order is big-endian") {
            conv->setArgs({"int32be(R0, R1)"});
            const ModbusRegisters input({0xfedc, 0xba98});

            MqttValue output = conv->toMqtt(input);

            REQUIRE(output.getDouble() == expected);
            REQUIRE(output.getString() == expectedString);
        }

        SECTION("and byte order is little-endian") {
            conv->setArgs({"int32le(R0, R1)"});
            const ModbusRegisters input({0xdcfe, 0x98ba});

            MqttValue output = conv->toMqtt(input);

            REQUIRE(output.getDouble() == expected);
            REQUIRE(output.getString() == expectedString);
        }
    }

    SECTION("when two registers contain an unsigned integer") {
        const uint32_t expected = 0xfedcba98;
        std::string expectedString = "4275878552";

        SECTION("and byte order is big-endian") {
            conv->setArgs({"uint32be(R0, R1)"});
            const ModbusRegisters input({0xfedc, 0xba98});

            MqttValue output = conv->toMqtt(input);

            REQUIRE(output.getDouble() == expected);
            REQUIRE(output.getString() == expectedString);
        }

        SECTION("and byte order is little-endian") {
            conv->setArgs({"uint32le(R0, R1)"});
            const ModbusRegisters input({0xdcfe, 0x98ba});

            MqttValue output = conv->toMqtt(input);

            REQUIRE(output.getDouble() == expected);
            REQUIRE(output.getString() == expectedString);
        }
    }
}

TEST_CASE("A float value should be converted by exprtk") {
    std::string stdconv_path = "../exprconv/exprconv.so";

    boost::shared_ptr<ConverterPlugin> plugin = boost_dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );
    std::shared_ptr<DataConverter> conv(plugin->getConverter("evaluate"));
    const float expected = -123.456f; // 0xc2f6e979 in IEEE 754 hex representation

    SECTION("when two registers contain the bytes in order ABCD") {
        conv->setArgs({"flt32abcd(R0, R1)"});
        const ModbusRegisters input({0xc2f6, 0xe979});

        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getDouble() == expected);
    }

    SECTION("when two registers contain the bytes in order BADC") {
        conv->setArgs({"flt32badc(R0, R1)"});
        const ModbusRegisters input({0xf6c2, 0x79e9});

        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getDouble() == expected);
    }

    SECTION("when two registers contain the bytes in order CDAB") {
        conv->setArgs({"flt32cdab(R0, R1)"});
        const ModbusRegisters input({0xe979, 0xc2f6});

        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getDouble() == expected);
    }

    SECTION("when two registers contain the bytes in order DCBA") {
        conv->setArgs({"flt32dcba(R0, R1)"});
        const ModbusRegisters input({0x79e9, 0xf6c2});

        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getDouble() == expected);
    }
}

#endif
