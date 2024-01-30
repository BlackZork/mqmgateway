#include <thread>

#include "catch2/catch_all.hpp"

#include "libmodmqttsrv/queue_item.hpp"
#include "libmodmqttsrv/modbus_poller.hpp"
#include "libmodmqttsrv/register_poll.hpp"
#include "libmodmqttsrv/modbus_types.hpp"


#include "modbus_utils.hpp"
#include "mockedmodbuscontext.hpp"
#include "../readerwriterqueue/readerwriterqueue.h"

TEST_CASE("ModbusPoller") {
    moodycamel::BlockingReaderWriterQueue<modmqttd::QueueItem> fromModbusQueue;
    MockedModbusFactory modbus_factory;

    modmqttd::ModbusPoller poller(fromModbusQueue);
    poller.init(modbus_factory.getContext("test"));

    ModbusPollerTestRegisters registers;
    std::chrono::steady_clock::duration waitTime;

    SECTION("should return zero duration for empty register set") {
        poller.setupInitialPoll(registers);

        waitTime = poller.pollNext();
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        REQUIRE(poller.allDone());

        poller.setPollList(registers);
        waitTime = poller.pollNext();
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        REQUIRE(poller.allDone());
    }

    SECTION("should immediately do initial poll for single register") {
        modbus_factory.setModbusRegisterValue("test",1,1,modmqttd::RegisterType::HOLDING, 5);

        auto reg = registers.add(1, 1);
        poller.setupInitialPoll(registers);
        waitTime = poller.pollNext();
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        REQUIRE(reg->getValues()[0] == 5);
        REQUIRE(poller.allDone());
    }

    SECTION("should poll every register on single slave list once") {
        modbus_factory.setModbusRegisterValue("test",1,1,modmqttd::RegisterType::HOLDING, 5);
        modbus_factory.setModbusRegisterValue("test",1,2,modmqttd::RegisterType::HOLDING, 6);

        auto reg1 = registers.add(1, 1);
        auto reg2 = registers.add(1, 2);

        poller.setupInitialPoll(registers);
        waitTime = poller.pollNext();
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        REQUIRE(reg1->getValues()[0] == 5);
        REQUIRE(!poller.allDone());

        waitTime = poller.pollNext();
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        REQUIRE(reg2->getValues()[0] == 6);
        REQUIRE(poller.allDone());
    }

    SECTION("should poll every register for multiple slaves once") {
        modbus_factory.setModbusRegisterValue("test",1,1,modmqttd::RegisterType::HOLDING, 5);
        modbus_factory.setModbusRegisterValue("test",2,20,modmqttd::RegisterType::HOLDING, 60);

        auto reg1 = registers.add(1, 1);
        auto reg2 = registers.add(2, 20);

        poller.setupInitialPoll(registers);
        waitTime = poller.pollNext();
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        REQUIRE(reg1->getValues()[0] == 5);
        REQUIRE(!poller.allDone());

        waitTime = poller.pollNext();
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        REQUIRE(reg2->getValues()[0] == 60);
        REQUIRE(poller.allDone());
    }

    SECTION("should not delay register read on initial poll") {
        modbus_factory.setModbusRegisterValue("test",1,1,modmqttd::RegisterType::HOLDING, 5);

        auto reg = registers.add(1, 1, std::chrono::milliseconds(5));
        poller.setupInitialPoll(registers);
        waitTime = poller.pollNext();
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        REQUIRE(reg->getValues()[0] == 5);
        REQUIRE(poller.allDone());
    }

    SECTION("after doing initial poll for single register") {
        modbus_factory.setModbusRegisterValue("test",1,1,modmqttd::RegisterType::HOLDING, 5);

        auto reg = registers.addDelayed(1, 1, std::chrono::milliseconds(5));
        poller.setupInitialPoll(registers);
        waitTime = poller.pollNext();
        REQUIRE(poller.allDone());

        SECTION("should delay register read on normal poll") {
            // need to wait because there is no silence between inital poll and
            // next poll
            poller.setPollList(registers);
            waitTime = poller.pollNext();
            REQUIRE(waitTime > std::chrono::milliseconds(4));
            REQUIRE(!poller.allDone());

            //simulate shorter wait than required
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            waitTime = poller.pollNext();
            REQUIRE(!poller.allDone());
            REQUIRE(std::chrono::milliseconds::zero() < waitTime);
            REQUIRE(waitTime < std::chrono::milliseconds(5));

            // required silence period reached
            std::this_thread::sleep_for(std::chrono::milliseconds(4));
            waitTime = poller.pollNext();
            REQUIRE(waitTime == std::chrono::milliseconds::zero());
            REQUIRE(reg->getValues()[0] == 5);
            REQUIRE(poller.allDone());
        }

        SECTION("should not delay register read if there was enough silence time") {
            std::this_thread::sleep_for(std::chrono::milliseconds(7));

            poller.setPollList(registers);
            waitTime = poller.pollNext();
            REQUIRE(waitTime == std::chrono::milliseconds::zero());
            REQUIRE(reg->getValues()[0] == 5);
            REQUIRE(poller.allDone());
        }
    }


    SECTION("after initial poll of two registers") {
        modbus_factory.setModbusRegisterValue("test",1,1,modmqttd::RegisterType::HOLDING, 1);
        modbus_factory.setModbusRegisterValue("test",2,20,modmqttd::RegisterType::HOLDING, 6);

        auto reg1 = registers.add(1, 1);
        auto reg2 = registers.addDelayed(2, 20, std::chrono::milliseconds(5));

        poller.setupInitialPoll(registers);
        poller.pollNext(); // 2.20 is polled first because it requires silence
        REQUIRE(reg2->getValues()[0] == 6);
        poller.pollNext();
        REQUIRE(reg1->getValues()[0] == 1);
        REQUIRE(poller.allDone());

        modbus_factory.setModbusRegisterValue("test",1,1,modmqttd::RegisterType::HOLDING, 10);
        modbus_factory.setModbusRegisterValue("test",2,20,modmqttd::RegisterType::HOLDING, 60);

        SECTION("should poll registers in order if there is not enough silence before poll") {
            poller.setPollList(registers);
            waitTime = poller.pollNext();
            REQUIRE(waitTime == std::chrono::milliseconds::zero());
            REQUIRE(reg1->getValues()[0] == 10);
            REQUIRE(!poller.allDone());

            waitTime = poller.pollNext();
            REQUIRE(waitTime > std::chrono::milliseconds(4));
            REQUIRE(!poller.allDone());

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            waitTime = poller.pollNext();
            REQUIRE(waitTime == std::chrono::milliseconds::zero());
            REQUIRE(reg2->getValues()[0] == 60);
            REQUIRE(poller.allDone());

        }

        SECTION("should poll waiting register first if there is enough silence before poll") {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            poller.setPollList(registers);
            waitTime = poller.pollNext();
            REQUIRE(waitTime == std::chrono::milliseconds::zero());
            REQUIRE(reg2->getValues()[0] == 60);
            REQUIRE(!poller.allDone());

            waitTime = poller.pollNext();
            REQUIRE(waitTime == std::chrono::milliseconds::zero());
            REQUIRE(reg1->getValues()[0] == 10);
            REQUIRE(poller.allDone());
        }
    }

    SECTION("after initial poll of three registers") {
        modbus_factory.setModbusRegisterValue("test",1,1,modmqttd::RegisterType::HOLDING, 1);
        modbus_factory.setModbusRegisterValue("test",2,20,modmqttd::RegisterType::HOLDING, 20);
        modbus_factory.setModbusRegisterValue("test",2,21,modmqttd::RegisterType::HOLDING, 21);

        auto reg1 = registers.add(1, 1);
        auto reg2 = registers.add(2, 20);
        auto reg3 = registers.addDelayed(2, 21, std::chrono::milliseconds(5));

        poller.setupInitialPoll(registers);
        poller.pollNext(); // 2.21 is polled first because it requires silence
        REQUIRE(reg3->getValues()[0] == 21);
        poller.pollNext(); // 2.20 is polled next because w group reads by slave
        REQUIRE(reg2->getValues()[0] == 20);
        poller.pollNext();
        REQUIRE(reg1->getValues()[0] == 1);
        REQUIRE(poller.allDone());

    }
}
