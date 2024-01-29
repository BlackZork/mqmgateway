#include <thread>

#include "catch2/catch_all.hpp"

#include "libmodmqttsrv/queue_item.hpp"
#include "libmodmqttsrv/modbus_poller.hpp"
#include "libmodmqttsrv/register_poll.hpp"
#include "libmodmqttsrv/modbus_types.hpp"


#include "modbus_utils.hpp"
#include "mockedmodbuscontext.hpp"
#include "../readerwriterqueue/readerwriterqueue.h"

TEST_CASE("ModbusPoller for single delay config") {
    moodycamel::BlockingReaderWriterQueue<modmqttd::QueueItem> fromModbusQueue;
    MockedModbusFactory modbus_factory;

    modmqttd::ModbusPoller poller(fromModbusQueue);
    poller.init(modbus_factory.getContext("test"));

    ModbusPollerTestRegisters registers;
    std::chrono::steady_clock::duration waitTime;

    modbus_factory.setModbusRegisterValue("test",1,1,modmqttd::RegisterType::HOLDING, 1);
    auto reg1 = registers.add(1, 1);

    SECTION("should poll single register without any delay") {
        poller.setSlaveDelay(1, std::chrono::milliseconds(50));

        poller.setupInitialPoll(registers);
        waitTime = poller.pollNext();
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        REQUIRE(poller.allDone());

        poller.setPollList(registers);
        waitTime = poller.pollNext();
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        REQUIRE(poller.allDone());
    }

    modbus_factory.setModbusRegisterValue("test",2,20,modmqttd::RegisterType::HOLDING, 6);
    auto reg2 = registers.add(2, 20);

    SECTION("should delay next poll if slave is changed") {
        poller.setSlaveDelay(1, std::chrono::milliseconds(15));

        poller.setupInitialPoll(registers);
        waitTime = poller.pollNext();
        REQUIRE(modbus_factory.getLastReadRegisterAddress() == std::tuple(1,1));

        waitTime = poller.pollNext();
        REQUIRE(modbus_factory.getLastReadRegisterAddress() == std::tuple(2,20));
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        REQUIRE(poller.allDone());

        //make some silence
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        poller.setPollList(registers);
        // reg1 should be polled first because we have silence to use
        // but poll of reg1 need to wait at least 10ms more, because reg2 was polled last
        waitTime = poller.pollNext();
        REQUIRE(waitTime > std::chrono::milliseconds(15));
        REQUIRE(!poller.allDone());

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        //poll of reg1 after silence
        waitTime = poller.pollNext();
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        REQUIRE(modbus_factory.getLastReadRegisterAddress() == std::tuple(1,1));
        REQUIRE(!poller.allDone());
        // poll of reg2
        waitTime = poller.pollNext();
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        REQUIRE(modbus_factory.getLastReadRegisterAddress() == std::tuple(2,20));
        REQUIRE(poller.allDone());
    }

    SECTION("should not delay next poll if slave was the same") {
        poller.setSlaveDelay(1, std::chrono::milliseconds(15));
        poller.setSlaveDelay(2, std::chrono::milliseconds(20));

        //initial poll selects reg2 due to longer delay needed
        poller.setupInitialPoll(registers);
        waitTime = poller.pollNext();
        REQUIRE(modbus_factory.getLastReadRegisterAddress() == std::tuple(2,20));
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        REQUIRE(!poller.allDone());

        waitTime = poller.pollNext();
        REQUIRE(modbus_factory.getLastReadRegisterAddress() == std::tuple(1,1));
        REQUIRE(poller.allDone());

        //make silence that can acomodate both delays
        std::this_thread::sleep_for(std::chrono::milliseconds(30));

        poller.setPollList(registers);
        waitTime = poller.pollNext();
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        //we choose to poll slave 1 because it was polled last
        REQUIRE(modbus_factory.getLastReadRegisterAddress() == std::tuple(1,1));
        REQUIRE(!poller.allDone());

        //reg2 needs silence due to slave change
        waitTime = poller.pollNext();
        REQUIRE(modbus_factory.getLastReadRegisterAddress() == std::tuple(2,20));
        REQUIRE(waitTime > std::chrono::milliseconds(10));
        REQUIRE(!poller.allDone());

        //make silence that can acomodate reg2
        std::this_thread::sleep_for(std::chrono::milliseconds(25));

        waitTime = poller.pollNext();
        REQUIRE(modbus_factory.getLastReadRegisterAddress() == std::tuple(2,20));
        REQUIRE(poller.allDone());
    }
}
