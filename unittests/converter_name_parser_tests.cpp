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

    SECTION("should throw ConvNameParserException if too many arguments are provided") {

        args.add("strval", ConverterArgType::STRING,"");

        REQUIRE_THROWS_AS(
            modmqttd::ConverterNameParser::parseArgs(args, "1,2"),
            modmqttd::ConvNameParserException
        );
    }
}

TEST_CASE("Converter params") {
    ConverterArgs args;

    args.add("a", ConverterArgType::STRING,"def");

    SECTION("should pass default value if not provided") {

        ConverterArgValues values = modmqttd::ConverterNameParser::parseArgs(args, "");

        REQUIRE(values.count() == 1);
        REQUIRE(values["a"].as_str()  == "def");
    }

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

    SECTION("should throw if param name is unknown") {
        REQUIRE_THROWS_AS(
            modmqttd::ConverterNameParser::parseArgs(args, "b=1"),
            modmqttd::ConvNameParserException
        );
    }

    SECTION("should throw if param name is empty") {
        REQUIRE_THROWS_AS(
            modmqttd::ConverterNameParser::parseArgs(args, "=1"),
            modmqttd::ConvNameParserException
        );
    }

    SECTION("should throw if there are too many assignment chars") {
        REQUIRE_THROWS_AS(
            modmqttd::ConverterNameParser::parseArgs(args, "a==1"),
            modmqttd::ConvNameParserException
        );
    }

    SECTION("should throw if param is set more than once") {
        REQUIRE_THROWS_AS(
            modmqttd::ConverterNameParser::parseArgs(args, "a=1, a=2"),
            modmqttd::ConvNameParserException
        );
    }
}


TEST_CASE("Converter args and params") {
    ConverterArgs args;

    args.add("arg", ConverterArgType::STRING,"a");
    args.add("param", ConverterArgType::STRING,"p");

    SECTION("should set default value if param is not provided") {
        ConverterArgValues values = modmqttd::ConverterNameParser::parseArgs(args, "arg=set");

        REQUIRE(values.count() == 2);
        REQUIRE(values["arg"].as_str() == "set");
        REQUIRE(values["param"].as_str() == "p");
    }

    SECTION("should throw if arg is after param") {
        REQUIRE_THROWS_AS(
            modmqttd::ConverterNameParser::parseArgs(args, "arg=2, 4"),
            modmqttd::ConvNameParserException
        );
    }

}
