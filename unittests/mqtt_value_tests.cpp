#include "catch2/catch_all.hpp"
#include <cfloat>
#include <cmath>
#include <limits>

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

TEST_CASE("MqttValue::fromLongDouble should") {
    SECTION("render a whole value as an integer") {
        MqttValue val(MqttValue::fromLongDouble(1.0L));

        REQUIRE("1" == val.getString());
    }

    SECTION("render a fractional value with default precision") {
        MqttValue val(MqttValue::fromLongDouble(1.1234L));

        REQUIRE("1.123400" == val.getString());
    }

    SECTION("render a fractional value with forced precision") {
        MqttValue val(MqttValue::fromLongDouble(1.1299L, 2));

        REQUIRE("1.13" == val.getString());
    }

    SECTION("render the same text as a double holding the same value") {
        REQUIRE(MqttValue::fromDouble(1.0).getString() == MqttValue::fromLongDouble(1.0L).getString());
        REQUIRE(MqttValue::fromDouble(1.1234).getString() == MqttValue::fromLongDouble(1.1234L).getString());
        REQUIRE(MqttValue::fromDouble(-5.0).getString() == MqttValue::fromLongDouble(-5.0L).getString());
        REQUIRE(MqttValue::fromDouble(1e20).getString() == MqttValue::fromLongDouble(1e20L).getString());
    }

#if LDBL_MANT_DIG >= 64
    SECTION("keep every digit of a whole value above 2^53") {
        // exactly representable only because long double has a 64 bit mantissa
        MqttValue val(MqttValue::fromLongDouble(18446744073709551615.0L));

        REQUIRE("18446744073709551615" == val.getString());
    }
#endif
}

TEST_CASE("MqttValue::asIntegral should") {
    SECTION("report a whole positive value as unsigned") {
        const auto integral = MqttValue::fromDouble(42.0).asIntegral();

        REQUIRE(integral.has_value());
        REQUIRE(!integral->isSigned());
        REQUIRE(42 == integral->asUInt64());
    }

    SECTION("report a whole negative value as signed") {
        const auto integral = MqttValue::fromLongDouble(-42.0L).asIntegral();

        REQUIRE(integral.has_value());
        REQUIRE(integral->isSigned());
        REQUIRE(-42 == integral->asInt64());
    }

    SECTION("report a value above int64 range as unsigned") {
        const auto integral =
            MqttValue::fromLongDouble(static_cast<long double>(INT64_MAX) + 1.0L).asIntegral();

        REQUIRE(integral.has_value());
        REQUIRE(!integral->isSigned());
    }

    SECTION("report nothing for a fractional value") {
        REQUIRE(!MqttValue::fromDouble(1.5).asIntegral().has_value());
    }

    SECTION("report nothing for a whole value outside both integer ranges") {
        REQUIRE(!MqttValue::fromDouble(1e20).asIntegral().has_value());
        REQUIRE(!MqttValue::fromDouble(-1e20).asIntegral().has_value());
    }

    SECTION("report nothing for a source type that is not floating point") {
        REQUIRE(!MqttValue::fromUInt64(7).asIntegral().has_value());
        REQUIRE(!MqttValue::fromString("7").asIntegral().has_value());
    }
}

TEST_CASE("MqttValue::IntegralValue should refuse") {
    SECTION("a signed read of an unsigned value") {
        const MqttValue::IntegralValue integral = MqttValue::IntegralValue::fromUInt64(1);

        REQUIRE_THROWS_AS(integral.asInt64(), std::logic_error);
    }

    SECTION("an unsigned read of a signed value") {
        const MqttValue::IntegralValue integral = MqttValue::IntegralValue::fromInt64(-1);

        REQUIRE_THROWS_AS(integral.asUInt64(), std::logic_error);
    }
}

TEST_CASE("MqttValue narrowing accessors should reject") {
    SECTION("an int that does not fit, read from uint64") {
        MqttValue val(MqttValue::fromUInt64(static_cast<uint64_t>(INT32_MAX) + 1));

        REQUIRE_THROWS_AS(val.getInt(), ConvException);
    }

    SECTION("an int that does not fit, read from a long double") {
        MqttValue val(MqttValue::fromLongDouble(1e30L));

        REQUIRE_THROWS_AS(val.getInt(), ConvException);
    }

    SECTION("an int64 that does not fit, read from a long double") {
        MqttValue val(MqttValue::fromLongDouble(1e30L));

        REQUIRE_THROWS_AS(val.getInt64(), ConvException);
    }

#if LDBL_MAX_EXP > DBL_MAX_EXP
    SECTION("a double that does not fit, read from a long double") {
        const long double biggest = std::numeric_limits<long double>::max();
        const MqttValue val = MqttValue::fromLongDouble(biggest);

        REQUIRE_THROWS_AS(val.getDouble(), ConvException);
    }
#endif

    SECTION("a not-a-number that cannot narrow to an int") {
        const long double nan = std::numeric_limits<long double>::quiet_NaN();
        const MqttValue val = MqttValue::fromLongDouble(nan);

        REQUIRE_THROWS_AS(val.getInt(), ConvException);
    }
}

TEST_CASE("MqttValue narrowing accessors should accept") {
    SECTION("a uint64 that fits an int") {
        REQUIRE(INT32_MAX == MqttValue::fromUInt64(INT32_MAX).getInt());
    }

    SECTION("a long double that fits an int") {
        REQUIRE(-7 == MqttValue::fromLongDouble(-7.0L).getInt());
    }

    SECTION("an infinity, which double can hold too") {
        const long double inf = std::numeric_limits<long double>::infinity();

        REQUIRE(std::isinf(MqttValue::fromLongDouble(inf).getDouble()));
    }
}
