#include "catch2/catch_all.hpp"
#include <yaml-cpp/yaml.h>

#include "libmodmqttsrv/yaml_converters.hpp"

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
