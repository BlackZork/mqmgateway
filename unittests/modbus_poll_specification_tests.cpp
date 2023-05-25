#include "catch2/catch.hpp"

#include "libmodmqttsrv/modbus_messages.hpp"

modmqttd::MsgRegisterPoll
createPoll(int firstRegister, int lastRegister, int refresh = 1) {
    modmqttd::MsgRegisterPoll ret;
    ret.mRegister = firstRegister;
    ret.mCount = lastRegister - firstRegister + 1;
    ret.mSlaveId = 1;
    ret.mRegisterType = modmqttd::RegisterType::INPUT;
    ret.mRefreshMsec = refresh;

    return ret;
}

TEST_CASE("MsgRegisterPollSpecification group tests") {
    modmqttd::MsgRegisterPollSpecification specs("test");

    SECTION("Group should not merge overlapping ranges") {

        specs.mRegisters.push_back(createPoll(1,3));
        specs.mRegisters.push_back(createPoll(2,4));

        specs.group();

        REQUIRE(specs.mRegisters.size() == 2);
    }

    SECTION ("Group should merge consecutive ranges") {
        modmqttd::MsgRegisterPollSpecification specs("test");

        specs.mRegisters.push_back(createPoll(1,2));
        specs.mRegisters.push_back(createPoll(3,4));

        specs.group();

        REQUIRE(specs.mRegisters.size() == 1);
        REQUIRE(specs.mRegisters.front().isSameAs(createPoll(1,4)));
    }

    SECTION ("Group should not merge disjoint ranges") {
        modmqttd::MsgRegisterPollSpecification specs("test");

        specs.mRegisters.push_back(createPoll(1,2));
        specs.mRegisters.push_back(createPoll(4,7));

        specs.group();

        REQUIRE(specs.mRegisters.size() == 2);
    }

    SECTION ("Group should merge multiple consecutive ranges with the lowest refresh time") {
        modmqttd::MsgRegisterPollSpecification specs("test");

        specs.mRegisters.push_back(createPoll(3,4,10));
        specs.mRegisters.push_back(createPoll(1,2,20));
        specs.mRegisters.push_back(createPoll(5,8,30));

        specs.group();

        REQUIRE(specs.mRegisters.size() == 1);
        REQUIRE(specs.mRegisters.front().isSameAs(createPoll(1,8)));
        REQUIRE(specs.mRegisters.front().mRefreshMsec == 10);
    }
}
