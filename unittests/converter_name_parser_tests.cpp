#include "catch2/catch_all.hpp"
#include <catch2/catch_test_macros.hpp>

#include "libmodmqttsrv/conv_name_parser.hpp"
#include "libmodmqttsrv/exceptions.hpp"

#include "libmodmqttconv/convargs.hpp"

TEST_CASE("Converter name") {

    ConverterArgs args;

    SECTION("should be parsed without args") {

        modmqttd::ConverterSpecification spec = modmqttd::ConverterNameParser::parse("std.something()");

        REQUIRE(spec.plugin == "std");
        REQUIRE(spec.converter == "something");
    }

    SECTION("should be parsed with spaces in empty args") {

        modmqttd::ConverterSpecification spec = modmqttd::ConverterNameParser::parse("std.something( )");

        REQUIRE(spec.plugin == "std");
        REQUIRE(spec.converter == "something");
    }
}

TEST_CASE("Converter arguments") {

    ConverterArgs args;

    SECTION("should be parsed with single int arg") {
        args.add("intval", ConverterArgType::INT, 0);
        ConverterArgValues values = modmqttd::ConverterNameParser::parseArgs(args, "1");

        REQUIRE(values.count() == 1);
        REQUIRE(values["intval"].as_str() == "1");
    }

    SECTION("should be parsed with single str arg") {

        args.add("strval", ConverterArgType::STRING, "");

        ConverterArgValues values = modmqttd::ConverterNameParser::parseArgs(args, "\"foo\"");

        REQUIRE(values.count() == 1);
        REQUIRE(values["strval"].as_str()  == "foo");
    }

    SECTION("should be parsed with with single str arg space ended") {

        args.add("strval", ConverterArgType::STRING, "");

        ConverterArgValues values = modmqttd::ConverterNameParser::parseArgs(args, "\"foo \"");

        REQUIRE(values.count() == 1);
        REQUIRE(values["strval"].as_str()  == "foo ");
    }

    SECTION("should be parsed with single int arg surrounded by spaces") {

        args.add("intval", ConverterArgType::INT, 0);

        ConverterArgValues values = modmqttd::ConverterNameParser::parseArgs(args, " 1 ");

        REQUIRE(values.count() == 1);
        REQUIRE(values["intval"].as_str()  == "1");
    }

    SECTION("should be parsed with two int args") {

        args.add("intval1", ConverterArgType::INT,0);
        args.add("intval2", ConverterArgType::INT,0);

        ConverterArgValues values = modmqttd::ConverterNameParser::parseArgs(args, "1,2");

        REQUIRE(values.count() == 2);
        REQUIRE(values["intval1"].as_str()  == "1");
        REQUIRE(values["intval2"].as_str()  == "2");
    }

    SECTION("should be parsed with two int and str args") {

        args.add("intval1", ConverterArgType::INT,0);
        args.add("strval2", ConverterArgType::STRING,"");

        ConverterArgValues values = modmqttd::ConverterNameParser::parseArgs(args, "1,\"2\"");

        REQUIRE(values.count() == 2);
        REQUIRE(values["intval1"].as_str()  == "1");
        REQUIRE(values["strval2"].as_str()  == "2");
    }

    SECTION("should be parsed with single quote arg") {

        args.add("strval", ConverterArgType::STRING,"");

        ConverterArgValues values = modmqttd::ConverterNameParser::parseArgs(args, "'1'");

        REQUIRE(values.count() == 1);
        REQUIRE(values["strval"].as_str()  == "1");
    }

    SECTION("should be parsed with single quote arg and double quote value") {

        args.add("strval", ConverterArgType::STRING,"");

        ConverterArgValues values = modmqttd::ConverterNameParser::parseArgs(args, "'\"'");

        REQUIRE(values.count() == 1);
        REQUIRE(values["strval"].as_str()  == "\"");
    }

    SECTION("should be parsed with double quote arg and single quote value") {

        args.add("strval", ConverterArgType::STRING,"");

        ConverterArgValues values = modmqttd::ConverterNameParser::parseArgs(args, "\"'\"");

        REQUIRE(values.count() == 1);
        REQUIRE(values["strval"].as_str()  == "'");
    }
}

TEST_CASE("Converter params") {
    ConverterArgs args;

    args.add("a", ConverterArgType::STRING,"");
    SECTION("should be parsed with single int param") {

        ConverterArgValues values = modmqttd::ConverterNameParser::parseArgs(args, "a=1");

        REQUIRE(values.count() == 1);
        REQUIRE(values["a"].as_str()  == "1");
    }

    SECTION("should be parsed with spaces between param and value") {

        ConverterArgValues values = modmqttd::ConverterNameParser::parseArgs(args, "a = 1");

        REQUIRE(values.count() == 1);
        REQUIRE(values["a"].as_str()  == "1");
    }

    SECTION("should throw exception for unknown param name") {
        REQUIRE_THROWS_AS(
            modmqttd::ConverterNameParser::parseArgs(args, "b=1"),
            modmqttd::ConvNameParserException
        );
    }

    SECTION("should throw exception for empty param name") {
        REQUIRE_THROWS_AS(
            modmqttd::ConverterNameParser::parseArgs(args, "=1"),
            modmqttd::ConvNameParserException
        );
    }

    SECTION("should throw exception for quoted param name") {
        REQUIRE_THROWS_AS(
            modmqttd::ConverterNameParser::parseArgs(args, "\"a\"=1"),
            modmqttd::ConvNameParserException
        );
    }

    SECTION("should throw exception for double assignment char") {
        REQUIRE_THROWS_AS(
            modmqttd::ConverterNameParser::parseArgs(args, " a ==1"),
            modmqttd::ConvNameParserException
        );
    }
}
