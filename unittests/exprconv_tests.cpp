#include <boost/dll/import.hpp>
#include <catch2/catch_all.hpp>
#include "libmodmqttconv/converterplugin.hpp"
#include "libmodmqttsrv/config.hpp"

#ifdef HAVE_EXPRTK

TEST_CASE ("A number should be converted by exprtk") {
    std::string stdconv_path = "../exprconv/exprconv.so";
    boost::shared_ptr<ConverterPlugin> plugin = boost_dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );
    std::shared_ptr<DataConverter> conv(plugin->getConverter("evaluate"));

    SECTION("when precision is not set") {
        conv->setArgs({"R0 * 2"});
        const ModbusRegisters input(10);

        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getString() == "20");
    }

    SECTION("when precision is set") {
        conv->setArgs({"R0 / 3", "3"});
        const ModbusRegisters input(10);

        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getString() == "3.333");
    }
}


TEST_CASE("A 32-bit number should be converted by exprtk") {
    std::string stdconv_path = "../exprconv/exprconv.so";
    boost::shared_ptr<ConverterPlugin> plugin = boost_dll_import<ConverterPlugin>(
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

    SECTION("when two registers contain a float") {
        const float expected = -123.456f; // 0xc2f6e979 in IEEE 754 hex representation
        const std::string expectedString = "-123.456001";

        SECTION("and byte order is ABCD") {
            conv->setArgs({"flt32be(R0, R1)"});
            const ModbusRegisters input({0xc2f6, 0xe979});

            MqttValue output = conv->toMqtt(input);

            REQUIRE_THAT(output.getDouble(), Catch::Matchers::WithinULP(expected, 0));
            REQUIRE(output.getString() == expectedString);
        }

        SECTION("and byte order is CDAB") {
            conv->setArgs({"flt32be(R1, R0)"});
            const ModbusRegisters input({0xe979, 0xc2f6});

            MqttValue output = conv->toMqtt(input);

            REQUIRE_THAT(output.getDouble(), Catch::Matchers::WithinULP(expected, 0));
            REQUIRE(output.getString() == expectedString);
        }

        SECTION("and byte order is BADC") {
            conv->setArgs({"flt32(R0, R1)"});
            const ModbusRegisters input({0xf6c2, 0x79e9});

            MqttValue output = conv->toMqtt(input);

            REQUIRE_THAT(output.getDouble(), Catch::Matchers::WithinULP(expected, 0));
            REQUIRE(output.getString() == expectedString);
        }

        SECTION("and byte order is DCBA") {
            conv->setArgs({"flt32(R1, R0)"});
            const ModbusRegisters input({0x79e9, 0xf6c2});

            MqttValue output = conv->toMqtt(input);

            REQUIRE_THAT(output.getDouble(), Catch::Matchers::WithinULP(expected, 0));
            REQUIRE(output.getString() == expectedString);
        }

        SECTION("and precision is set") {
            conv->setArgs({"flt32be(R0, R1)", "3"});
            const ModbusRegisters input({0xc2f6, 0xe979});

            MqttValue output = conv->toMqtt(input);

            REQUIRE(output.getString() == "-123.456");
        }
    }
}

TEST_CASE ("A uint16_t register data should be converted to exprtk value") {
    std::string stdconv_path = "../exprconv/exprconv.so";
    boost::shared_ptr<ConverterPlugin> plugin = boost_dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );
    std::shared_ptr<DataConverter> conv(plugin->getConverter("evaluate"));

    conv->setArgs({"int16(R0)"});
    const ModbusRegisters input(0xFFFF);

    MqttValue output = conv->toMqtt(input);

    REQUIRE(output.getString() == "-1");
}


#endif
