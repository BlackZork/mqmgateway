#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>

#include "libmodmqttconv/converterplugin.hpp"
#include "libmodmqttsrv/config.hpp"

#include "testnumbers.hpp"
#include "plugin_utils.hpp"

#ifdef HAVE_EXPRTK

TEST_CASE ("Expr.evaluate should") {
    PluginLoader loader("../exprconv/exprconv.so");

    std::shared_ptr<DataConverter> conv(loader.getConverter("evaluate"));
    ConverterArgValues args(conv->getArgs());

    SECTION("convert an int value to a single modbus register") {
        args.setArgValue("expression", "M0*2");

        MqttValue input(12);

        conv->setArgValues(args);
        ModbusRegisters output = conv->toModbus(input, 1);

        REQUIRE(output.getCount() == 1);
        REQUIRE(output.getValue(0) == 24);
    }


    SECTION("cast negative int value to uint16_t") {
        args.setArgValue("expression", "M0-1");
        args.setArgValue("write_as", "int16");

        MqttValue input(-1);

        conv->setArgValues(args);
        ModbusRegisters output = conv->toModbus(input, 1);

        REQUIRE(output.getCount() == 1);
        REQUIRE(output.getValue(0) == 0xFFFE);
    }


    SECTION("convert a single element table to a single modbus register") {
        args.setArgValue("expression", "return [M0*2]");

        MqttValue input(12);

        conv->setArgValues(args);
        ModbusRegisters output = conv->toModbus(input, 1);

        REQUIRE(output.getCount() == 1);
        REQUIRE(output.getValue(0) == 24);
    }


    SECTION("store two items results table to two registers") {
        args.setArgValue("expression", "return [M0+1, M0-1]");

        MqttValue input(12);

        conv->setArgValues(args);
        ModbusRegisters output = conv->toModbus(input, 2);

        REQUIRE(output.getCount() == 2);
        REQUIRE(output.getValue(0) == 13);
        REQUIRE(output.getValue(1) == 11);
    }


    SECTION("throw if number of registers do not match") {
        args.setArgValue("expression", "M0+1");

        MqttValue input(12);

        conv->setArgValues(args);

        REQUIRE_THROWS_AS(
            conv->toModbus(input, 2),
            ConvException
        );
    }


    SECTION("throw if returned value is a string") {
        args.setArgValue("expression", "return ['str']");

        MqttValue input(12);

        conv->setArgValues(args);

        REQUIRE_THROWS_AS(
            conv->toModbus(input, 1),
            ConvException
        );
    }

    SECTION("throw evaluated value is a string") {
        args.setArgValue("expression", "'str'");

        MqttValue input(12);

        conv->setArgValues(args);

        REQUIRE_THROWS_AS(
            conv->toModbus(input, 1),
            ConvException
        );
    }

    SECTION("throw if int value is out uint16_t range") {
        args.setArgValue("expression", "M0+1");

        MqttValue input(0xFFFF);

        conv->setArgValues(args);

        REQUIRE_THROWS_AS(
            conv->toModbus(input, 1),
            ConvException
        );
    }

    // 5e9 is above UINT32_MAX. The value reaches the writer as a long double and
    // is narrowed before the range check that exists to catch it, so today the
    // out of range value is written rather than reported.
    SECTION("throw if value is out of uint32 range") {
        args.setArgValue("expression", "M0");
        args.setArgValue("write_as", "uint32");

        MqttValue input(static_cast<int64_t>(5000000000));

        conv->setArgValues(args);

        REQUIRE_THROWS_AS(conv->toModbus(input, 2), ConvException);
    }

    SECTION("throw if value is out of int32 range") {
        args.setArgValue("expression", "M0");
        args.setArgValue("write_as", "int32");

        MqttValue input(static_cast<int64_t>(5000000000));

        conv->setArgValues(args);

        REQUIRE_THROWS_AS(conv->toModbus(input, 2), ConvException);
    }

    SECTION("throw if write helper has bad name") {
        args.setArgValue("expression", "M0+1");
        args.setArgValue("write_as", "int15");

        REQUIRE_THROWS_AS(
            conv->setArgValues(args),
            ConvException
        );
    }

    SECTION("write a not-a-number result as float32 bits") {
        args.setArgValue("expression", "M0");
        args.setArgValue("write_as", "flt32");

        conv->setArgValues(args);
        const ModbusRegisters output = conv->toModbus(MqttValue::fromString("NaN"), 2);

        REQUIRE(output.values() == std::vector<uint16_t>({TestNumbers::Float::NAN_HIGH, TestNumbers::Float::NAN_LOW}));
    }

    SECTION("write a not-a-number result as float64 bits") {
        args.setArgValue("expression", "M0");
        args.setArgValue("write_as", "flt64");

        conv->setArgValues(args);
        const ModbusRegisters output = conv->toModbus(MqttValue::fromString("NaN"), 4);

        const uint16_t* nan = TestNumbers::Double::NAN_REGISTERS;
        REQUIRE(output.values() == std::vector<uint16_t>(nan, nan + 4));
    }

    SECTION("write a not-a-number fall-through value of an expression that can return a list") {
        args.setArgValue("expression", "if (M0 > 100) { return [M0, M0]; }; M0");
        args.setArgValue("write_as", "flt32");

        conv->setArgValues(args);
        const ModbusRegisters output = conv->toModbus(MqttValue::fromString("NaN"), 2);

        REQUIRE(output.values() == std::vector<uint16_t>({TestNumbers::Float::NAN_HIGH, TestNumbers::Float::NAN_LOW}));
    }

    SECTION("reject a not-a-number result written as an integer") {
        args.setArgValue("expression", "M0");
        args.setArgValue("write_as", "int16");

        conv->setArgValues(args);

        REQUIRE_THROWS_WITH(conv->toModbus(MqttValue::fromString("NaN"), 1), Catch::Matchers::ContainsSubstring("out of int range"));
    }

    SECTION("reject a not-a-number passed to a register function") {
        args.setArgValue("expression", "int16(M0)");
        args.setArgValue("write_as", "int16");

        conv->setArgValues(args);

        REQUIRE_THROWS_AS(conv->toModbus(MqttValue::fromString("NaN"), 1), ConvException);
    }

    SECTION("reject a value outside the register range passed to a register function") {
        args.setArgValue("expression", "int16(M0)");
        args.setArgValue("write_as", "int16");

        conv->setArgValues(args);

        REQUIRE_THROWS_AS(conv->toModbus(MqttValue::fromInt(70000), 1), ConvException);
    }

    SECTION("reject an infinity passed to a four register function") {
        args.setArgValue("expression", "flt64(M0, 0, 0, 0)");
        args.setArgValue("write_as", "flt64");

        conv->setArgValues(args);

        REQUIRE_THROWS_AS(conv->toModbus(MqttValue::fromString("Infinity"), 4), ConvException);
    }

    SECTION("evaluate again after a register function rejected its argument") {
        args.setArgValue("expression", "int16(M0)");
        args.setArgValue("write_as", "int16");

        conv->setArgValues(args);
        REQUIRE_THROWS_AS(conv->toModbus(MqttValue::fromString("NaN"), 1), ConvException);

        const ModbusRegisters output = conv->toModbus(MqttValue::fromInt(5), 1);

        REQUIRE(output.values() == std::vector<uint16_t>({5}));
    }
}

#endif
