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
