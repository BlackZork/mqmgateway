#include <thread>

#include "catch2/catch_all.hpp"

#include "libmodmqttsrv/queue_item.hpp"
#include "libmodmqttsrv/modbus_executor.hpp"
#include "libmodmqttsrv/register_poll.hpp"
#include "libmodmqttsrv/modbus_types.hpp"


#include "modbus_utils.hpp"
#include "mockedmodbuscontext.hpp"
#include "../readerwriterqueue/readerwriterqueue.h"

TEST_CASE("ModbusExecutor for single delay config") {
    moodycamel::BlockingReaderWriterQueue<modmqttd::QueueItem> fromModbusQueue;
    MockedModbusFactory modbus_factory;

    modmqttd::ModbusExecutor executor(fromModbusQueue);
    executor.init(modbus_factory.getContext("test"));

    ModbusExecutorTestRegisters registers;
    std::chrono::steady_clock::duration waitTime;

    modbus_factory.setModbusRegisterValue("test",1,1,modmqttd::RegisterType::HOLDING, 1);

    SECTION("should poll single register without any delay") {
        auto reg1 = registers.addDelayed(1, 1, std::chrono::milliseconds(50), modmqttd::RegisterPoll::ReadDelayType::FIRST_READ);

        executor.setupInitialPoll(registers);
        waitTime = executor.pollNext();
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        REQUIRE(executor.allDone());

        executor.setPollList(registers);
        waitTime = executor.pollNext();
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        REQUIRE(executor.allDone());
    }

    modbus_factory.setModbusRegisterValue("test",2,20,modmqttd::RegisterType::HOLDING, 6);

    SECTION("should delay next poll if slave is changed") {
        auto reg1 = registers.addDelayed(1, 1, std::chrono::milliseconds(15), modmqttd::RegisterPoll::ReadDelayType::FIRST_READ);
        auto reg2 = registers.add(2, 20);

        executor.setupInitialPoll(registers);
        waitTime = executor.pollNext();
        REQUIRE(modbus_factory.getLastReadRegisterAddress() == std::tuple(1,1));

        waitTime = executor.pollNext();
        REQUIRE(modbus_factory.getLastReadRegisterAddress() == std::tuple(2,20));
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        REQUIRE(executor.allDone());

        //make some silence
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        executor.setPollList(registers);
        // reg1 should be polled first because we have silence to use
        // but poll of reg1 need to wait about 10ms more, because reg2 was polled last
        waitTime = executor.pollNext();
        REQUIRE(waitTime > std::chrono::milliseconds(5));
        REQUIRE(!executor.allDone());

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        //poll of reg1 after silence
        waitTime = executor.pollNext();
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        REQUIRE(modbus_factory.getLastReadRegisterAddress() == std::tuple(1,1));
        REQUIRE(!executor.allDone());
        // poll of reg2
        waitTime = executor.pollNext();
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        REQUIRE(modbus_factory.getLastReadRegisterAddress() == std::tuple(2,20));
        REQUIRE(executor.allDone());
    }

    SECTION("should not delay next poll if slave was the same") {
        auto reg1 = registers.addDelayed(1, 1, std::chrono::milliseconds(15), modmqttd::RegisterPoll::ReadDelayType::FIRST_READ);
        auto reg2 = registers.addDelayed(2, 20, std::chrono::milliseconds(20), modmqttd::RegisterPoll::ReadDelayType::FIRST_READ);

        //initial poll selects reg2 due to longer delay needed
        executor.setupInitialPoll(registers);
        waitTime = executor.pollNext();
        REQUIRE(modbus_factory.getLastReadRegisterAddress() == std::tuple(2,20));
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        REQUIRE(!executor.allDone());

        //slave changed, reg1 needs 15 ms delay
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        waitTime = executor.pollNext();
        REQUIRE(modbus_factory.getLastReadRegisterAddress() == std::tuple(1,1));
        REQUIRE(executor.allDone());

        //make silence that can acomodate both delays
        std::this_thread::sleep_for(std::chrono::milliseconds(30));

        executor.setPollList(registers);
        waitTime = executor.pollNext();
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        //we choose to poll slave 1 because it was polled last
        REQUIRE(modbus_factory.getLastReadRegisterAddress() == std::tuple(1,1));
        REQUIRE(!executor.allDone());

        //reg2 needs silence due to slave change
        waitTime = executor.pollNext();
        REQUIRE(waitTime > std::chrono::milliseconds(10));
        REQUIRE(!executor.allDone());

        //make silence that can acomodate reg2
        std::this_thread::sleep_for(std::chrono::milliseconds(25));

        waitTime = executor.pollNext();
        REQUIRE(modbus_factory.getLastReadRegisterAddress() == std::tuple(2,20));
        REQUIRE(executor.allDone());
    }
}
