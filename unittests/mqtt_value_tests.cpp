#include "catch2/catch_all.hpp"
#include "libmodmqttconv/mqttvalue.hpp"


TEST_CASE("MqttValue::fromDouble should output") {
    SECTION("value without trailing zeroes if there is no fractional part") {
        MqttValue val(MqttValue::fromDouble(1.0));

        std::string strval(val.getString());
        REQUIRE("1" == strval);
    }

    SECTION("value with precision number of zeroes if there is no fractional part") {
        MqttValue val(MqttValue::fromDouble(1.0, 2));

        std::string strval(val.getString());
        REQUIRE("1.00" == strval);
    }

    SECTION("value with default std::precision if there is fractional part") {
        MqttValue val(MqttValue::fromDouble(1.1234));

        std::string strval(val.getString());
        REQUIRE("1.123400" == strval);
    }

    SECTION("value rounded down with forced precision if there is fractional part") {
        MqttValue val(MqttValue::fromDouble(1.1234, 2));

        std::string strval(val.getString());
        REQUIRE("1.12" == strval);
    }

    SECTION("value rouned up with forced precision if there is fractional part") {
        MqttValue val(MqttValue::fromDouble(1.1299, 2));

        std::string strval(val.getString());
        REQUIRE("1.13" == strval);
    }

    SECTION("whole value larger than int64 range without wrapping to a negative") {
        MqttValue val(MqttValue::fromDouble(1e20));

        std::string strval(val.getString());
        REQUIRE("100000000000000000000" == strval);
    }
}

TEST_CASE("MqttValue::setBinary should set") {
    SECTION("binary size to the number of copied bytes") {
        MqttValue val;
        val.setBinary("abc", 3);

        REQUIRE(3 == val.getBinarySize());
        REQUIRE("abc" == val.getString());
    }
}

TEST_CASE("MqttValue::fromString should parse") {
    SECTION("int value in decimal format") {
        MqttValue strval(MqttValue::fromString("10"));

        int intval = strval.getInt();
        REQUIRE(10 == intval);
    }
    SECTION("int value in hex format") {
        MqttValue strval(MqttValue::fromString("0x10"));

        int intval = strval.getInt();
        REQUIRE(16 == intval);
    }
    SECTION("int value in ocatl format") {
        MqttValue strval(MqttValue::fromString("010"));

        int intval = strval.getInt();
        REQUIRE(8 == intval);
    }
}

TEST_CASE("MqttValue::fromUInt64 should") {
    SECTION("output a value that does not fit int64") {
        MqttValue val(MqttValue::fromUInt64(UINT64_MAX));

        REQUIRE("18446744073709551615" == val.getString());
        REQUIRE(UINT64_MAX == val.getUInt64());
        REQUIRE(MqttValue::SourceType::UINT64 == val.getSourceType());
    }

    SECTION("throw when read as int64 and the value does not fit") {
        MqttValue val(MqttValue::fromUInt64(static_cast<uint64_t>(INT64_MAX) + 1));

        REQUIRE_THROWS_AS(val.getInt64(), ConvException);
    }

    SECTION("read as int64 when the value fits") {
        MqttValue val(MqttValue::fromUInt64(static_cast<uint64_t>(INT64_MAX)));

        REQUIRE(INT64_MAX == val.getInt64());
    }
}

TEST_CASE("MqttValue::getUInt64 should") {
    SECTION("parse a decimal string that does not fit int64") {
        MqttValue val(MqttValue::fromString("18446744073709551615"));

        REQUIRE(UINT64_MAX == val.getUInt64());
    }

    SECTION("reject a negative string instead of wrapping it") {
        MqttValue val(MqttValue::fromString("-1"));

        REQUIRE_THROWS_AS(val.getUInt64(), ConvException);
    }

    SECTION("reject a string that is not a number") {
        MqttValue val(MqttValue::fromString("nope"));

        REQUIRE_THROWS_AS(val.getUInt64(), ConvException);
    }

    SECTION("reject a negative int") {
        MqttValue val(MqttValue::fromInt(-1));

        REQUIRE_THROWS_AS(val.getUInt64(), ConvException);
    }

    SECTION("reject a double outside uint64 range") {
        MqttValue val(MqttValue::fromDouble(-1.0));

        REQUIRE_THROWS_AS(val.getUInt64(), ConvException);
    }
}
