#include "catch2/catch_all.hpp"
#include <yaml-cpp/yaml.h>

#include "libmodmqttsrv/yaml_converters.hpp"

typedef std::vector<std::pair<int,int>> pairs;

TEST_CASE ("time string should") {
    SECTION("be parsed as milliseconds") {
        YAML::Node node = YAML::Load("100ms");
        std::chrono::milliseconds ts = node.as<std::chrono::milliseconds>();
        REQUIRE(ts == std::chrono::milliseconds(100));
    }

    SECTION("be parsed as seconds") {
        YAML::Node node = YAML::Load("3s");
        std::chrono::milliseconds ts = node.as<std::chrono::milliseconds>();
        REQUIRE(ts == std::chrono::milliseconds(1000 * 3));
    }

    SECTION("be parsed as milliseconds") {
        YAML::Node node = YAML::Load("1min");
        std::chrono::milliseconds ts = node.as<std::chrono::milliseconds>();
        REQUIRE(ts == std::chrono::milliseconds(60*1000));
    }

    SECTION("throw config exception if unparsable") {
        YAML::Node node = YAML::Load("30a");
        REQUIRE_THROWS_AS(node.as<std::chrono::milliseconds>(), modmqttd::ConfigurationException);
    }
}


TEST_CASE ("number list string should") {
    SECTION("be parsed as single number") {
        YAML::Node node = YAML::Load("4");
        pairs lst = node.as<pairs>();
        REQUIRE(lst[0] == std::pair(4,4));
    }

    SECTION("be parsed as single number with spaces ignored") {
        YAML::Node node = YAML::Load(" 4");
        pairs lst = node.as<pairs>();
        REQUIRE(lst[0] == std::pair(4,4));
    }

    SECTION("fail if value is not a number") {
        YAML::Node node = YAML::Load("a");
        REQUIRE_THROWS_AS(node.as<pairs>(), modmqttd::ConfigurationException);
    }

    SECTION("fail if value contains invalid characters") {
        YAML::Node node = YAML::Load("4 z");
        REQUIRE_THROWS_AS(node.as<pairs>(), modmqttd::ConfigurationException);
    }


    SECTION("be parsed as list of number separated by comma") {
        YAML::Node node = YAML::Load("4,5");
        pairs lst = node.as<pairs>();
        REQUIRE(lst[0] == std::pair(4,4));
        REQUIRE(lst[1] == std::pair(5,5));
    }

    SECTION("be parsed as list of number ranges separated by comma") {
        YAML::Node node = YAML::Load("4-6,5-18");
        pairs lst = node.as<pairs>();
        REQUIRE(lst[0] == std::pair(4,6));
        REQUIRE(lst[1] == std::pair(5,18));
    }

    SECTION("should ignore spaces") {
        YAML::Node node = YAML::Load(" 4, 5-18, 7");
        pairs lst = node.as<pairs>();
        REQUIRE(lst[0] == std::pair(4,4));
        REQUIRE(lst[1] == std::pair(5,18));
        REQUIRE(lst[2] == std::pair(7,7));
    }

}


TEST_CASE ("string list string should") {
    SECTION("be parsed for single string") {
        YAML::Node node = YAML::Load("a");
        std::vector<std::string> lst = node.as<std::vector<std::string>>();
        REQUIRE(lst[0] == "a");
    }

    SECTION("ignore spaces") {
        YAML::Node node = YAML::Load(" a, b ");
        std::vector<std::string> lst = node.as<std::vector<std::string>>();
        REQUIRE(lst[0] == "a");
        REQUIRE(lst[1] == "b");
    }

}
