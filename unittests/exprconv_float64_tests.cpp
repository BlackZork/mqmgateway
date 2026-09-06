#include <catch2/catch_all.hpp>
#include "libmodmqttconv/converterplugin.hpp"
#include "libmodmqttsrv/config.hpp"
#include "libmodmqttsrv/dll_import.hpp"

#include "testnumbers.hpp"
#include "plugin_utils.hpp"

#ifdef HAVE_EXPRTK

TEST_CASE("exprtk should read float64") {
    PluginLoader loader("../exprconv/exprconv.so");

    std::shared_ptr<DataConverter> conv(loader.getConverter("evaluate"));
    ConverterArgValues args(conv->getArgs());

    SECTION("from four registers") {
        args.setArgValue("expression", "flt64(R0, R1, R2, R3)");
        const ModbusRegisters input({TestNumbers::Double::AB, TestNumbers::Double::CD, TestNumbers::Double::EF, TestNumbers::Double::GH});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE_THAT(output.getDouble(), Catch::Matchers::WithinULP(TestNumbers::Double::ABCDEFGH_as_double, 0));
    }

    SECTION("from four registers byte swapped") {
        args.setArgValue("expression", "flt64bs(R0, R1, R2, R3)");
        const ModbusRegisters input({TestNumbers::Double::BA, TestNumbers::Double::DC, TestNumbers::Double::FE, TestNumbers::Double::HG});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE_THAT(output.getDouble(), Catch::Matchers::WithinULP(TestNumbers::Double::ABCDEFGH_as_double, 0));
    }

    SECTION("from four registers in reverse word order") {
        args.setArgValue("expression", "flt64(R3, R2, R1, R0)");
        const ModbusRegisters input({TestNumbers::Double::GH, TestNumbers::Double::EF, TestNumbers::Double::CD, TestNumbers::Double::AB});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE_THAT(output.getDouble(), Catch::Matchers::WithinULP(TestNumbers::Double::ABCDEFGH_as_double, 0));
    }

    SECTION("from four registers in reverse word order byte swapped") {
        args.setArgValue("expression", "flt64bs(R3, R2, R1, R0)");
        const ModbusRegisters input({TestNumbers::Double::HG, TestNumbers::Double::FE, TestNumbers::Double::DC, TestNumbers::Double::BA});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE_THAT(output.getDouble(), Catch::Matchers::WithinULP(TestNumbers::Double::ABCDEFGH_as_double, 0));
    }

    SECTION("and round to the requested precision") {
        args.setArgValue("expression", "flt64(R0, R1, R2, R3)");
        args.setArgValue(ConverterArg::sPrecisionArgName, "3");
        const ModbusRegisters input({TestNumbers::Double::AB, TestNumbers::Double::CD, TestNumbers::Double::EF, TestNumbers::Double::GH});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getString() == "-1.235");
    }

    SECTION("and carry an infinity through") {
        args.setArgValue("expression", "flt64(R0, R1, R2, R3)");
        const ModbusRegisters input(std::vector<uint16_t>(std::begin(TestNumbers::Double::POS_INF_REGISTERS),
                                                          std::end(TestNumbers::Double::POS_INF_REGISTERS)));

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(std::isinf(output.getDouble()));
        REQUIRE(output.getDouble() > 0);
    }

    SECTION("and carry a negative infinity through") {
        args.setArgValue("expression", "flt64(R0, R1, R2, R3)");
        const ModbusRegisters input(std::vector<uint16_t>(std::begin(TestNumbers::Double::NEG_INF_REGISTERS),
                                                          std::end(TestNumbers::Double::NEG_INF_REGISTERS)));

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(std::isinf(output.getDouble()));
        REQUIRE(output.getDouble() < 0);
    }

    SECTION("and carry a not-a-number through") {
        args.setArgValue("expression", "flt64(R0, R1, R2, R3)");
        const ModbusRegisters input(std::vector<uint16_t>(std::begin(TestNumbers::Double::NAN_REGISTERS),
                                                          std::end(TestNumbers::Double::NAN_REGISTERS)));

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(std::isnan(output.getDouble()));
    }
}

#endif
