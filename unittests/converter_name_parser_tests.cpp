#include <regex>

#include "catch2/catch.hpp"
#include "libmodmqttsrv/conv_name_parser.hpp"

TEST_CASE("Parse converter name without args") {

    modmqttd::ConverterSpecification spec = modmqttd::ConverterNameParser::parse("std.something()");

    REQUIRE(spec.plugin == "std");
    REQUIRE(spec.converter == "something");
    REQUIRE(spec.args.size() == 0);
}

TEST_CASE("Parse converter name with spaces in args") {

    modmqttd::ConverterSpecification spec = modmqttd::ConverterNameParser::parse("std.something( )");

    REQUIRE(spec.plugin == "std");
    REQUIRE(spec.converter == "something");
    REQUIRE(spec.args.size() == 0);
}

TEST_CASE("Parse converter name with single int arg") {

    modmqttd::ConverterSpecification spec = modmqttd::ConverterNameParser::parse("std.something(1)");

    REQUIRE(spec.plugin == "std");
    REQUIRE(spec.converter == "something");
    REQUIRE(spec.args.size() == 1);
    REQUIRE(spec.args.front() == "1");
}

TEST_CASE("Parse converter name with single str arg") {

    modmqttd::ConverterSpecification spec = modmqttd::ConverterNameParser::parse("std.something(\"foo\")");

    REQUIRE(spec.plugin == "std");
    REQUIRE(spec.converter == "something");
    REQUIRE(spec.args.size() == 1);
    REQUIRE(spec.args.front() == "foo");
}

TEST_CASE("Parse converter name with single str arg space ended") {

    modmqttd::ConverterSpecification spec = modmqttd::ConverterNameParser::parse("std.something(\"foo \")");

    REQUIRE(spec.plugin == "std");
    REQUIRE(spec.converter == "something");
    REQUIRE(spec.args.size() == 1);
    REQUIRE(spec.args.front() == "foo ");
}

TEST_CASE("Parse converter name with single int arg surrounded by spaces") {

    modmqttd::ConverterSpecification spec = modmqttd::ConverterNameParser::parse("std.something( 1 )");

    REQUIRE(spec.plugin == "std");
    REQUIRE(spec.converter == "something");
    REQUIRE(spec.args.size() == 1);
    REQUIRE(spec.args.front() == "1");
}

TEST_CASE("Parse converter name with two int args") {

    modmqttd::ConverterSpecification spec = modmqttd::ConverterNameParser::parse("std.something(1,2)");

    REQUIRE(spec.plugin == "std");
    REQUIRE(spec.converter == "something");
    REQUIRE(spec.args.size() == 2);
    std::vector<std::string> args = { "1", "2"};
    REQUIRE(spec.args == args);
}

TEST_CASE("Parse converter name with two int and str args") {

    modmqttd::ConverterSpecification spec = modmqttd::ConverterNameParser::parse("std.something(1,\"2\")");

    REQUIRE(spec.plugin == "std");
    REQUIRE(spec.converter == "something");
    REQUIRE(spec.args.size() == 2);
    std::vector<std::string> args = { "1", "2"};
    REQUIRE(spec.args == args);
}



