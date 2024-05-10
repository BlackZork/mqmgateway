#include <thread>

#include "catch2/catch_all.hpp"

#include "libmodmqttsrv/queue_item.hpp"
#include "libmodmqttsrv/modbus_executor.hpp"
#include "libmodmqttsrv/register_poll.hpp"
#include "libmodmqttsrv/modbus_types.hpp"


#include "modbus_utils.hpp"
#include "mockedmodbuscontext.hpp"
#include "../readerwriterqueue/readerwriterqueue.h"

TEST_CASE("ModbusExecutor for first delay config") {
    moodycamel::BlockingReaderWriterQueue<modmqttd::QueueItem> fromModbusQueue;
    moodycamel::BlockingReaderWriterQueue<modmqttd::QueueItem> toModbusQueue;
    MockedModbusFactory modbus_factory;

    modmqttd::ModbusExecutor executor(fromModbusQueue, toModbusQueue);
    executor.init(modbus_factory.getContext("test"));

    ModbusExecutorTestRegisters registers;
    std::chrono::steady_clock::duration waitTime;

    modbus_factory.setModbusRegisterValue("test",1,1,modmqttd::RegisterType::HOLDING, 1);

    SECTION("should poll single register without any delay") {
        auto reg1 = registers.addPollDelayed(1, 1, std::chrono::milliseconds(50), modmqttd::ModbusCommandDelay::DelayType::ON_SLAVE_CHANGE);

        executor.setupInitialPoll(registers);
        waitTime = executor.executeNext();
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        REQUIRE(executor.allDone());

        executor.addPollList(registers);
        waitTime = executor.executeNext();
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        REQUIRE(executor.allDone());
    }

    modbus_factory.setModbusRegisterValue("test",2,20,modmqttd::RegisterType::HOLDING, 6);

    SECTION("should delay next poll if slave is changed") {
        auto reg1 = registers.addPollDelayed(1, 1, std::chrono::milliseconds(15), modmqttd::ModbusCommandDelay::DelayType::ON_SLAVE_CHANGE);
        auto reg2 = registers.addPoll(2, 20);

        executor.setupInitialPoll(registers);
        waitTime = executor.executeNext();
        REQUIRE(modbus_factory.getLastReadRegisterAddress() == std::tuple<int, int>(1,1));

        waitTime = executor.executeNext();
        REQUIRE(modbus_factory.getLastReadRegisterAddress() == std::tuple<int, int>(2,20));
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        REQUIRE(executor.allDone());

        //make some silence
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        executor.addPollList(registers);
        // reg1 should be polled first because we have silence to use
        // but poll of reg1 need to wait about 10ms more, because reg2 was polled last
        waitTime = executor.executeNext();
        REQUIRE(waitTime > std::chrono::milliseconds(5));
        REQUIRE(!executor.allDone());

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        //poll of reg1 after silence
        waitTime = executor.executeNext();
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        REQUIRE(modbus_factory.getLastReadRegisterAddress() == std::tuple<int, int>(1,1));
        REQUIRE(!executor.allDone());
        // poll of reg2
        waitTime = executor.executeNext();
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        REQUIRE(modbus_factory.getLastReadRegisterAddress() == std::tuple<int, int>(2,20));
        REQUIRE(executor.allDone());
    }

    SECTION("should not delay next poll if slave was the same") {
        auto reg1 = registers.addPollDelayed(1, 1, std::chrono::milliseconds(15), modmqttd::ModbusCommandDelay::DelayType::ON_SLAVE_CHANGE);
        auto reg2 = registers.addPollDelayed(2, 20, std::chrono::milliseconds(20), modmqttd::ModbusCommandDelay::DelayType::ON_SLAVE_CHANGE);

        //initial poll selects reg2 due to longer delay needed
        executor.setupInitialPoll(registers);
        waitTime = executor.executeNext();
        REQUIRE(modbus_factory.getLastReadRegisterAddress() == std::tuple<int, int>(2,20));
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        REQUIRE(!executor.allDone());

        //slave changed, reg1 needs 15 ms delay
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        waitTime = executor.executeNext();
        REQUIRE(modbus_factory.getLastReadRegisterAddress() == std::tuple<int, int>(1,1));
        REQUIRE(executor.allDone());

        //make silence that can acomodate both delays
        std::this_thread::sleep_for(std::chrono::milliseconds(30));

        executor.addPollList(registers);
        waitTime = executor.executeNext();
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        //we choose to poll slave 2 because it better fits into available delay
        REQUIRE(modbus_factory.getLastReadRegisterAddress() == std::tuple<int, int>(2,20));
        REQUIRE(!executor.allDone());

        //reg1 needs silence due to slave change
        waitTime = executor.executeNext();
        REQUIRE(waitTime > std::chrono::milliseconds(5));
        REQUIRE(!executor.allDone());

        //make silence that can acomodate reg1
        std::this_thread::sleep_for(std::chrono::milliseconds(15));

        waitTime = executor.executeNext();
        REQUIRE(modbus_factory.getLastReadRegisterAddress() == std::tuple<int, int>(1,1));
        REQUIRE(executor.allDone());
    }

    SECTION("should not wipe current command when adding next pollList with command that needs to wait") {
        // execute poll to clean silence period
        auto reg1 = registers.addPoll(1, 1);
        executor.setupInitialPoll(registers);
        executor.executeNext();

        // add write about to be executed
        auto write = registers.createWriteDelayed(1,0x15, std::chrono::milliseconds(30));
        executor.addWriteCommand(1, write);
        waitTime = executor.executeNext();
        REQUIRE(executor.getWaitingCommand() == write);

        // bug - should not drop waiting command when adding new command that needs wait
        reg1 = registers.addPoll(1, 1, std::chrono::milliseconds(50));
        executor.setupInitialPoll(registers);
        REQUIRE(executor.getWaitingCommand() == write);
    }

}
