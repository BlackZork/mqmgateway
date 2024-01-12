#include "catch2/catch_all.hpp"

#include "libmodmqttsrv/modbus_messages.hpp"

modmqttd::MsgRegisterPoll
createPoll(int firstRegister, int lastRegister, int refresh = 1) {
    modmqttd::MsgRegisterPoll ret(firstRegister);
    ret.mCount = lastRegister - firstRegister + 1;
    ret.mSlaveId = 1;
    ret.mRegisterType = modmqttd::RegisterType::INPUT;
    ret.mRefreshMsec = std::chrono::milliseconds(refresh);

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
        REQUIRE(specs.mRegisters.front().mRefreshMsec == std::chrono::milliseconds(10));
    }
}


TEST_CASE("MsgRegisterPollSpecification merge tests") {
    modmqttd::MsgRegisterPollSpecification specs("test");

    SECTION("Merge of overlapping registers should work") {

        specs.mRegisters.push_back(createPoll(1,3));

        specs.merge(createPoll(2,4));

        REQUIRE(specs.mRegisters.size() == 1);
        REQUIRE(specs.mRegisters.front().isSameAs(createPoll(1,4)));
    }

    SECTION("Merge of containing range should work") {

        specs.mRegisters.push_back(createPoll(1,8));

        specs.merge(createPoll(2,4));

        REQUIRE(specs.mRegisters.size() == 1);
        REQUIRE(specs.mRegisters.front().isSameAs(createPoll(1,8)));
    }

    SECTION("Merge of extending range should work") {

        specs.mRegisters.push_back(createPoll(6,8));

        specs.merge(createPoll(1,10));

        REQUIRE(specs.mRegisters.size() == 1);
        REQUIRE(specs.mRegisters.front().isSameAs(createPoll(1,10)));
    }

    SECTION("Glue two disjoint ranges with overlapping register") {

        specs.mRegisters.push_back(createPoll(1,3));
        specs.mRegisters.push_back(createPoll(6,8));

        specs.merge(createPoll(2,7));

        REQUIRE(specs.mRegisters.size() == 1);
        REQUIRE(specs.mRegisters.front().isSameAs(createPoll(1,8)));
    }


    SECTION("Merge should not join consecutive registers") {

        specs.mRegisters.push_back(createPoll(1,3));

        specs.merge(createPoll(4,5));

        REQUIRE(specs.mRegisters.size() == 2);
    }

    SECTION("Glue one of two disjoint ranges with overlapping and consecutive register") {

        specs.mRegisters.push_back(createPoll(1,3));
        specs.mRegisters.push_back(createPoll(6,8));

        specs.merge(createPoll(4,7));

        REQUIRE(specs.mRegisters.size() == 2);
        REQUIRE(specs.mRegisters[0].isSameAs(createPoll(1,3)));
        REQUIRE(specs.mRegisters[1].isSameAs(createPoll(4,8)));
    }

}
