#include <libmodmqttsrv/config.hpp>
#include "catch2/catch_all.hpp"
#include "libmodmqttsrv/dll_import.hpp"

#include "libmodmqttconv/converterplugin.hpp"
#include "libmodmqttconv/convexception.hpp"

#include "testnumbers.hpp"
#include "plugin_utils.hpp"

TEST_CASE("A float64 value should be read") {
    PluginLoader loader("../stdconv/stdconv.so");

    std::shared_ptr<DataConverter> conv(loader.getConverter("float64"));
    ConverterArgValues args(conv->getArgs());

    SECTION("when four registers contain a double in ABCDEFGH format") {
        const ModbusRegisters input({TestNumbers::Double::AB, TestNumbers::Double::CD, TestNumbers::Double::EF, TestNumbers::Double::GH});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE_THAT(output.getDouble(), Catch::Matchers::WithinULP(TestNumbers::Double::ABCDEFGH_as_double, 0));
        REQUIRE(output.getString() == std::to_string(TestNumbers::Double::ABCDEFGH_as_double));
    }

    SECTION("when four registers contain a double in GHEFCDAB format") {
        const ModbusRegisters input({TestNumbers::Double::GH, TestNumbers::Double::EF, TestNumbers::Double::CD, TestNumbers::Double::AB});

        args.setArgValue(ConverterArg::sLowFirstArgName, "true");
        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE_THAT(output.getDouble(), Catch::Matchers::WithinULP(TestNumbers::Double::ABCDEFGH_as_double, 0));
    }

    SECTION("when four registers contain a double in BADCFEHG format") {
        const ModbusRegisters input({TestNumbers::Double::BA, TestNumbers::Double::DC, TestNumbers::Double::FE, TestNumbers::Double::HG});

        args.setArgValue(ConverterArg::sSwapBytesArgName, "true");
        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE_THAT(output.getDouble(), Catch::Matchers::WithinULP(TestNumbers::Double::ABCDEFGH_as_double, 0));
    }

    SECTION("when four registers contain a double in HGFEDCBA format") {
        const ModbusRegisters input({TestNumbers::Double::HG, TestNumbers::Double::FE, TestNumbers::Double::DC, TestNumbers::Double::BA});

        args.setArgValue(ConverterArg::sLowFirstArgName, "true");
        args.setArgValue(ConverterArg::sSwapBytesArgName, "true");
        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE_THAT(output.getDouble(), Catch::Matchers::WithinULP(TestNumbers::Double::ABCDEFGH_as_double, 0));
    }

    SECTION("when precision is set") {
        const ModbusRegisters input({TestNumbers::Double::AB, TestNumbers::Double::CD, TestNumbers::Double::EF, TestNumbers::Double::GH});

        args.setArgValue(ConverterArg::sPrecisionArgName, "2");
        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getString() == "-1.23");
    }

    SECTION("when registers hold nan or infinity") {
        conv->setArgValues(args);

        const ModbusRegisters nanInput(std::vector<uint16_t>(std::begin(TestNumbers::Double::NAN_REGISTERS), std::end(TestNumbers::Double::NAN_REGISTERS)));
        REQUIRE(std::isnan(conv->toMqtt(nanInput).getDouble()));

        const ModbusRegisters posInfInput(std::vector<uint16_t>(std::begin(TestNumbers::Double::POS_INF_REGISTERS), std::end(TestNumbers::Double::POS_INF_REGISTERS)));
        REQUIRE(std::isinf(conv->toMqtt(posInfInput).getDouble()));
        REQUIRE(conv->toMqtt(posInfInput).getDouble() > 0.0);

        const ModbusRegisters negInfInput(std::vector<uint16_t>(std::begin(TestNumbers::Double::NEG_INF_REGISTERS), std::end(TestNumbers::Double::NEG_INF_REGISTERS)));
        REQUIRE(std::isinf(conv->toMqtt(negInfInput).getDouble()));
        REQUIRE(conv->toMqtt(negInfInput).getDouble() < 0.0);
    }

    SECTION("but rejects any register count other than four") {
        conv->setArgValues(args);

        REQUIRE_THROWS_AS(conv->toMqtt(ModbusRegisters({TestNumbers::Double::AB, TestNumbers::Double::CD})), ConvException);
        REQUIRE_THROWS_AS(
            conv->toMqtt(ModbusRegisters({TestNumbers::Double::AB, TestNumbers::Double::CD, TestNumbers::Double::EF, TestNumbers::Double::GH, TestNumbers::Double::AB})),
            ConvException);
    }
}

TEST_CASE("A float64 value should be written") {
    PluginLoader loader("../stdconv/stdconv.so");

    std::shared_ptr<DataConverter> conv(loader.getConverter("float64"));
    ConverterArgValues args(conv->getArgs());

    // all 17 significant digits, so parsing the payload back yields exactly the
    // double the registers hold - a float32 could not carry this value
    std::string val("-1.2345678901234567");
    MqttValue input = MqttValue::fromBinary(val.c_str(), val.length());

    SECTION("to four registers in ABCDEFGH format") {
        conv->setArgValues(args);
        const ModbusRegisters converted = conv->toModbus(input, 4);
        const ModbusRegisters expected({TestNumbers::Double::AB, TestNumbers::Double::CD, TestNumbers::Double::EF, TestNumbers::Double::GH});

        REQUIRE(converted.values() == expected.values());
    }

    SECTION("to four registers in GHEFCDAB format") {
        args.setArgValue(ConverterArg::sLowFirstArgName, "true");
        conv->setArgValues(args);
        const ModbusRegisters converted = conv->toModbus(input, 4);
        const ModbusRegisters expected({TestNumbers::Double::GH, TestNumbers::Double::EF, TestNumbers::Double::CD, TestNumbers::Double::AB});

        REQUIRE(converted.values() == expected.values());
    }

    SECTION("to four registers in BADCFEHG format") {
        args.setArgValue(ConverterArg::sSwapBytesArgName, "true");
        conv->setArgValues(args);
        const ModbusRegisters converted = conv->toModbus(input, 4);
        const ModbusRegisters expected({TestNumbers::Double::BA, TestNumbers::Double::DC, TestNumbers::Double::FE, TestNumbers::Double::HG});

        REQUIRE(converted.values() == expected.values());
    }

    SECTION("to four registers in HGFEDCBA format") {
        args.setArgValue(ConverterArg::sLowFirstArgName, "true");
        args.setArgValue(ConverterArg::sSwapBytesArgName, "true");
        conv->setArgValues(args);
        const ModbusRegisters converted = conv->toModbus(input, 4);
        const ModbusRegisters expected({TestNumbers::Double::HG, TestNumbers::Double::FE, TestNumbers::Double::DC, TestNumbers::Double::BA});

        REQUIRE(converted.values() == expected.values());
    }

    SECTION("but not to any register count other than four") {
        conv->setArgValues(args);

        REQUIRE_THROWS_AS(conv->toModbus(input, 2), ConvException);
        REQUIRE_THROWS_AS(conv->toModbus(input, 5), ConvException);
    }
}
