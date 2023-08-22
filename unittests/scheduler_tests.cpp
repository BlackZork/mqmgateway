#include "catch2/catch.hpp"
#include "libmodmqttsrv/modbus_scheduler.hpp"
#include <iostream>

typedef std::map<int, std::vector<std::shared_ptr<modmqttd::RegisterPoll>>> RegisterSpec;

TEST_CASE( "Modbus scheduler basic tests" ) {
    modmqttd::ModbusScheduler scheduler;
    std::chrono::nanoseconds duration = std::chrono::seconds(1000);
    std::chrono::time_point<std::chrono::steady_clock> timePoint;

    SECTION ("No registers should return empty list to poll") {
        RegisterSpec source;
        REQUIRE(scheduler.getRegistersToPoll(source, duration, timePoint).size() == 0);
    }
}

TEST_CASE( "Modbus scheduler single register tests" ) {
    modmqttd::ModbusScheduler scheduler;
    std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();

    RegisterSpec source;
    std::shared_ptr<modmqttd::RegisterPoll> reg(new modmqttd::RegisterPoll(1, modmqttd::RegisterType::BIT, 1, 1000));
    reg->mRefresh = std::chrono::milliseconds(1000);
    source[0].push_back(reg);

    std::chrono::nanoseconds duration = std::chrono::seconds(1000);

    SECTION ("poll register now and wait 1sec for next poll") {
        reg->mLastRead = now - std::chrono::milliseconds(1000);
        RegisterSpec poll = scheduler.getRegistersToPoll(source, duration, now);

        CHECK(duration == std::chrono::milliseconds(1000));
        REQUIRE(poll.size() == 1);
    }

    SECTION ("wait 800ms to poll register") {
        reg->mLastRead = now - std::chrono::milliseconds(200);
        RegisterSpec poll = scheduler.getRegistersToPoll(source, duration, now);

        CHECK(duration == std::chrono::milliseconds(800));
        REQUIRE(poll.size() == 0);
    }

}
