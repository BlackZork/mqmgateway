#include <thread>

#include "catch2/catch_all.hpp"

#include "libmodmqttsrv/queue_item.hpp"
#include "libmodmqttsrv/modbus_poller.hpp"
#include "libmodmqttsrv/register_poll.hpp"
#include "libmodmqttsrv/modbus_types.hpp"


#include "mockedmodbuscontext.hpp"
#include "../readerwriterqueue/readerwriterqueue.h"

class TestRegisters : public std::map<int, std::vector<std::shared_ptr<modmqttd::RegisterPoll>>>
{
    public:
        std::shared_ptr<modmqttd::RegisterPoll> add(
            int slave,
            int number,
            std::chrono::steady_clock::duration delayBeforePoll = std::chrono::milliseconds::zero(),
            std::chrono::milliseconds refresh = std::chrono::milliseconds(10)
        ) {
            //TODO no check if already on list
            std::shared_ptr<modmqttd::RegisterPoll> reg(new modmqttd::RegisterPoll(number-1, modmqttd::RegisterType::HOLDING, 1, refresh));
            reg->mDelayBeforePoll = delayBeforePoll;
            (*this)[slave].push_back(reg);
            return reg;
        }
};


TEST_CASE("ModbusPoller") {
    moodycamel::BlockingReaderWriterQueue<modmqttd::QueueItem> fromModbusQueue;
    MockedModbusFactory modbus_factory;

    modmqttd::ModbusPoller poller(fromModbusQueue);
    poller.init(modbus_factory.getContext("test"));

    TestRegisters registers;

    SECTION("should return zero duration for empty register set") {
        poller.setupInitialPoll(registers);

        auto waitTime = poller.pollNext();
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
        auto waitTime = poller.pollNext();
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
        auto waitTime = poller.pollNext();
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
        auto waitTime = poller.pollNext();
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
        auto waitTime = poller.pollNext();
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        REQUIRE(reg->getValues()[0] == 5);
        REQUIRE(poller.allDone());
    }

    SECTION("after doing initial poll for single register") {
        modbus_factory.setModbusRegisterValue("test",1,1,modmqttd::RegisterType::HOLDING, 5);

        auto reg = registers.add(1, 1, std::chrono::milliseconds(5));
        poller.setupInitialPoll(registers);
        auto waitTime = poller.pollNext();
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


    SECTION("after initial poll for slave two registers") {
        modbus_factory.setModbusRegisterValue("test",1,1,modmqttd::RegisterType::HOLDING, 5);
        modbus_factory.setModbusRegisterValue("test",2,20,modmqttd::RegisterType::HOLDING, 60);

        auto reg1 = registers.add(1, 1);
        auto reg2 = registers.add(2, 20, std::chrono::milliseconds(5));

        poller.setupInitialPoll(registers);
        auto waitTime = poller.pollNext();
        waitTime = poller.pollNext();
        REQUIRE(poller.allDone());
    }
}
