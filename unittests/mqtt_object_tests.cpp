#include <catch2/catch_all.hpp>

#include "libmodmqttsrv/mqttobject.hpp"

using namespace modmqttd;

/**
 * The every_poll republish gate compares modbus read start times, which is
 * what ModbusScheduler times the next poll from. Nothing here waits, so these
 * are plain std::chrono values: the timestamps stand in for reads the
 * scheduler has already spaced apart.
 */
TEST_CASE("An every_poll object") {
    MqttObject obj("test_sensor");
    obj.setPublishMode(PublishMode::EVERY_POLL, std::chrono::milliseconds(100));

    const std::chrono::steady_clock::time_point firstRead = std::chrono::steady_clock::now();

    SECTION("should republish for the first read after startup") {
        REQUIRE(obj.needStateRepublish(firstRead));
    }

    // the scheduler elects a register as soon as one refresh period has passed
    // since the read started, so this is the smallest gap between two polls
    // that can ever reach the main thread
    SECTION("should republish for a read exactly one period after the last published one") {
        obj.setLastPublishedReadTime(firstRead);
        REQUIRE(obj.needStateRepublish(firstRead + std::chrono::milliseconds(100)));
    }

    SECTION("should not republish for a register arriving within the same poll round") {
        obj.setLastPublishedReadTime(firstRead);
        REQUIRE(!obj.needStateRepublish(firstRead + std::chrono::milliseconds(5)));
    }

    SECTION("should not republish for values that did not come from a read") {
        obj.setLastPublishedReadTime(firstRead);
        REQUIRE(!obj.needStateRepublish(std::chrono::steady_clock::time_point::min()));
    }
}

TEST_CASE("An object that is not every_poll") {
    MqttObject obj("test_sensor");
    const std::chrono::steady_clock::time_point read = std::chrono::steady_clock::now();

    SECTION("should never republish on_change") {
        obj.setPublishMode(PublishMode::ON_CHANGE, std::chrono::milliseconds(100));
        REQUIRE(!obj.needStateRepublish(read + std::chrono::seconds(10)));
    }

    SECTION("should never republish once") {
        obj.setPublishMode(PublishMode::ONCE, std::chrono::milliseconds(100));
        REQUIRE(!obj.needStateRepublish(read + std::chrono::seconds(10)));
    }
}
