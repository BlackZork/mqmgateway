#include <catch2/catch_all.hpp>
#include "libmodmqttconv/converterplugin.hpp"
#include "libmodmqttsrv/dll_import.hpp"

#include "testnumbers.hpp"

#ifdef HAVE_EXPRTK

TEST_CASE("exprtk should read float from two registers") {
    std::string stdconv_path = "../exprconv/exprconv.so";
    std::shared_ptr<ConverterPlugin> plugin = modmqttd::boost_dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );
    std::shared_ptr<DataConverter> conv(plugin->getConverter("evaluate"));

    const float expected = -123.456f; // 0xc2f6e979 in IEEE 754 hex representation
    const std::string expectedString = "-123.456001";


    SECTION("when byte order is ABCD") {
        conv->setArgs({"flt32(R0, R1)"});
        const ModbusRegisters input({TestNumbers::Float::AB, TestNumbers::Float::CD});

        MqttValue output = conv->toMqtt(input);

        REQUIRE_THAT(output.getDouble(), Catch::Matchers::WithinULP(TestNumbers::Float::ABCD_as_float, 0));
        REQUIRE(output.getString() == std::to_string(TestNumbers::Float::ABCD_as_float));
    }

    SECTION("and byte order is CDAB") {
        conv->setArgs({"flt32(R1, R0)"});
        const ModbusRegisters input({TestNumbers::Float::AB, TestNumbers::Float::CD});

        MqttValue output = conv->toMqtt(input);

        REQUIRE_THAT(output.getDouble(), Catch::Matchers::WithinULP(TestNumbers::Float::ABCD_as_float, 0));
        REQUIRE(output.getString() == std::to_string(TestNumbers::Float::ABCD_as_float));
    }

    SECTION("and byte order is BADC") {
        conv->setArgs({"flt32bs(R0, R1)"});
        const ModbusRegisters input({TestNumbers::Float::BA, TestNumbers::Float::DC});

        MqttValue output = conv->toMqtt(input);

        REQUIRE_THAT(output.getDouble(), Catch::Matchers::WithinULP(TestNumbers::Float::ABCD_as_float, 0));
        REQUIRE(output.getString() == std::to_string(TestNumbers::Float::ABCD_as_float));
    }

    SECTION("and byte order is DCBA") {
        conv->setArgs({"flt32bs(R1, R0)"});
        const ModbusRegisters input({TestNumbers::Float::BA, TestNumbers::Float::DC});

        MqttValue output = conv->toMqtt(input);

        REQUIRE_THAT(output.getDouble(), Catch::Matchers::WithinULP(TestNumbers::Float::ABCD_as_float, 0));
        REQUIRE(output.getString() == std::to_string(TestNumbers::Float::ABCD_as_float));
    }

    SECTION("and precision is set") {
        conv->setArgs({"flt32(R0, R1)", "3"});
        const ModbusRegisters input({0xc2f6, 0xe979});

        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getString() == "-123.456");
    }
}

#endif
