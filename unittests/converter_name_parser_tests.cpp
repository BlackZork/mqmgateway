#include "catch2/catch_all.hpp"
#include <catch2/catch_test_macros.hpp>

#include "libmodmqttsrv/conv_name_parser.hpp"
#include "libmodmqttsrv/exceptions.hpp"

#include "libmodmqttconv/convargs.hpp"

TEST_CASE("Converter name") {

    ConverterArgs args;

    SECTION("should be parsed without args") {

        modmqttd::ConverterSpecification spec = modmqttd::ConverterNameParser::parse(args, "std.something()");

        REQUIRE(spec.plugin == "std");
        REQUIRE(spec.converter == "something");
        REQUIRE(spec.getArgCount() == 0);
    }

    SECTION("should be parsed with spaces in empty args") {

        modmqttd::ConverterSpecification spec = modmqttd::ConverterNameParser::parse(args, "std.something( )");

        REQUIRE(spec.plugin == "std");
        REQUIRE(spec.converter == "something");
        REQUIRE(spec.getArgCount() == 0);
    }
}

TEST_CASE("Converter arguments") {

    ConverterArgs args;

    SECTION("should be parsed with single int arg") {

        args.add("intval", ConverterArgType::INT);

        modmqttd::ConverterSpecification spec = modmqttd::ConverterNameParser::parse(args, "std.something(1)");

        REQUIRE(spec.getArgCount() == 1);
        REQUIRE(spec["intval"] == "1");
    }

    SECTION("should be parsed with single str arg") {

        args.add("strval", ConverterArgType::STRING);

        modmqttd::ConverterSpecification spec = modmqttd::ConverterNameParser::parse(args, "std.something(\"foo\")");

        REQUIRE(spec.getArgCount() == 1);
        REQUIRE(spec["strval"] == "foo");
    }

    SECTION("should be parsed with with single str arg space ended") {

        args.add("strval", ConverterArgType::STRING);

        modmqttd::ConverterSpecification spec = modmqttd::ConverterNameParser::parse(args, "std.something(\"foo \")");

        REQUIRE(spec.getArgCount() == 1);
        REQUIRE(spec["strval"] == "foo");
    }

    SECTION("should be parsed with single int arg surrounded by spaces") {

        args.add("intval", ConverterArgType::INT);

        modmqttd::ConverterSpecification spec = modmqttd::ConverterNameParser::parse(args, "std.something( 1 )");

        REQUIRE(spec.getArgCount() == 1);
        REQUIRE(spec["intval"] == "1");
    }

    SECTION("should be parsed with two int args") {

        args.add("intval1", ConverterArgType::INT);
        args.add("intval2", ConverterArgType::INT);

        modmqttd::ConverterSpecification spec = modmqttd::ConverterNameParser::parse(args, "std.something(1,2)");

        REQUIRE(spec.getArgCount() == 2);
        REQUIRE(spec["intval1"] == "1");
        REQUIRE(spec["intval2"] == "2");
    }

    SECTION("should be parsed with two int and str args") {

        args.add("intval1", ConverterArgType::INT);
        args.add("strval2", ConverterArgType::STRING);

        modmqttd::ConverterSpecification spec = modmqttd::ConverterNameParser::parse(args, "std.something(1,\"2\")");

        REQUIRE(spec.getArgCount() == 2);
        REQUIRE(spec["intval1"] == "1");
        REQUIRE(spec["strval2"] == "2");
    }

    SECTION("should be parsed with single quote arg") {

        args.add("strval", ConverterArgType::STRING);

        modmqttd::ConverterSpecification spec = modmqttd::ConverterNameParser::parse(args, "std.something('1')");

        REQUIRE(spec.getArgCount() == 1);
        REQUIRE(spec["strval"] == "1");
    }

    SECTION("should be parsed with single quote arg and double quote value") {

        args.add("strval", ConverterArgType::STRING);

        modmqttd::ConverterSpecification spec = modmqttd::ConverterNameParser::parse(args, "std.something('\"')");

        REQUIRE(spec.getArgCount() == 1);
        REQUIRE(spec["strval"] == "\"");
    }

    SECTION("should be parsed with double quote arg and single quote value") {

        args.add("strval", ConverterArgType::STRING);

        modmqttd::ConverterSpecification spec = modmqttd::ConverterNameParser::parse(args, "std.something(\"'\")");

        REQUIRE(spec.getArgCount() == 1);
        REQUIRE(spec["strval"] == "'");
    }
}

TEST_CASE("Converter params") {
    ConverterArgs args;

    SECTION("should be parsed with single int param") {
        args.add("a", ConverterArgType::STRING);

        modmqttd::ConverterSpecification spec = modmqttd::ConverterNameParser::parse(args, "std.something(a=1)");

        REQUIRE(spec.getArgCount() == 1);
        REQUIRE(spec["a"] == "1");
    }

    SECTION("should throw exception for unknown param name") {
        args.add("a", ConverterArgType::STRING);

        REQUIRE_THROWS_AS(
            modmqttd::ConverterNameParser::parse(args, "std.something(b=1)"),
            modmqttd::ConvNameParserException
        );
    }
}
