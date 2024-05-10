#include "catch2/catch_all.hpp"
#include "libmodmqttsrv/modbus_scheduler.hpp"
#include <iostream>

typedef std::map<int, std::vector<std::shared_ptr<modmqttd::RegisterPoll>>> RegisterSpec;

TEST_CASE("Empty modbus scheduler") {
    modmqttd::ModbusScheduler scheduler;
    std::chrono::nanoseconds duration = std::chrono::seconds(1000);
    std::chrono::time_point<std::chrono::steady_clock> timePoint;

    SECTION ("should return empty list to poll") {
        auto regs = scheduler.getRegistersToPoll(duration, timePoint);
        REQUIRE(regs.size() == 0);
        REQUIRE(duration == std::chrono::steady_clock::duration::max());
    }
}

TEST_CASE("Modbus scheduler") {
    std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();

    RegisterSpec source;
    std::shared_ptr<modmqttd::RegisterPoll> reg(new modmqttd::RegisterPoll(1, 1, modmqttd::RegisterType::BIT, 1, std::chrono::milliseconds(1000)));
    source[reg->mSlaveId].push_back(reg);

    std::chrono::nanoseconds duration = std::chrono::seconds(1000);

    modmqttd::ModbusScheduler scheduler;
    scheduler.setPollSpecification(source);

    SECTION ("should return register and delay=1sec for next poll") {
        reg->mLastRead = now - std::chrono::milliseconds(1000);
        RegisterSpec poll = scheduler.getRegistersToPoll(duration, now);

        CHECK(duration == std::chrono::milliseconds(1000));
        REQUIRE(poll.size() == 1);
    }

    SECTION ("should return delay=800ms to poll register") {
        reg->mLastRead = now - std::chrono::milliseconds(200);
        RegisterSpec poll = scheduler.getRegistersToPoll(duration, now);

        CHECK(duration == std::chrono::milliseconds(800));
        REQUIRE(poll.size() == 0);
    }

    SECTION ("should return delay=mRefresh if register was pulled now") {
        reg->mLastRead = now;
        RegisterSpec poll = scheduler.getRegistersToPoll(duration, now);

        CHECK(duration == reg->mRefresh);
        REQUIRE(poll.size() == 0);
    }

}
