#include <thread>

#include "catch2/catch_all.hpp"

#include "libmodmqttsrv/queue_item.hpp"
#include "libmodmqttsrv/modbus_executor.hpp"
#include "libmodmqttsrv/register_poll.hpp"
#include "libmodmqttsrv/modbus_types.hpp"


#include "modbus_utils.hpp"
#include "mockedmodbuscontext.hpp"
#include "../readerwriterqueue/readerwriterqueue.h"

TEST_CASE("ModbusExecutor") {
    moodycamel::BlockingReaderWriterQueue<modmqttd::QueueItem> fromModbusQueue;
    moodycamel::BlockingReaderWriterQueue<modmqttd::QueueItem> toModbusQueue;

    MockedModbusFactory modbus_factory;

    modmqttd::ModbusExecutor executor(fromModbusQueue, toModbusQueue);
    executor.init(modbus_factory.getContext("test"));

    ModbusExecutorTestRegisters registers;
    std::chrono::steady_clock::duration waitTime;

    SECTION("should return zero duration for empty register set") {
        executor.setupInitialPoll(registers);

        waitTime = executor.pollNext();
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        REQUIRE(executor.allDone());

        executor.addPollList(registers);
        waitTime = executor.pollNext();
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        REQUIRE(executor.allDone());
    }

    SECTION("should immediately do initial poll for single register") {
        modbus_factory.setModbusRegisterValue("test",1,1,modmqttd::RegisterType::HOLDING, 5);

        auto reg = registers.addPoll(1, 1);
        executor.setupInitialPoll(registers);
        waitTime = executor.pollNext();
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        REQUIRE(reg->getValues()[0] == 5);
        REQUIRE(executor.allDone());
    }

    SECTION("should poll every register on single slave list once") {
        modbus_factory.setModbusRegisterValue("test",1,1,modmqttd::RegisterType::HOLDING, 5);
        modbus_factory.setModbusRegisterValue("test",1,2,modmqttd::RegisterType::HOLDING, 6);

        auto reg1 = registers.addPoll(1, 1);
        auto reg2 = registers.addPoll(1, 2);

        executor.setupInitialPoll(registers);
        waitTime = executor.pollNext();
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        REQUIRE(reg1->getValues()[0] == 5);
        REQUIRE(!executor.allDone());

        waitTime = executor.pollNext();
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        REQUIRE(reg2->getValues()[0] == 6);
        REQUIRE(executor.allDone());
    }

    SECTION("should poll every register for multiple slaves once") {
        modbus_factory.setModbusRegisterValue("test",1,1,modmqttd::RegisterType::HOLDING, 5);
        modbus_factory.setModbusRegisterValue("test",2,20,modmqttd::RegisterType::HOLDING, 60);

        auto reg1 = registers.addPoll(1, 1);
        auto reg2 = registers.addPoll(2, 20);

        executor.setupInitialPoll(registers);
        waitTime = executor.pollNext();
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        REQUIRE(reg1->getValues()[0] == 5);
        REQUIRE(!executor.allDone());

        waitTime = executor.pollNext();
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        REQUIRE(reg2->getValues()[0] == 60);
        REQUIRE(executor.allDone());
    }

    SECTION("should not delay register read on initial poll") {
        modbus_factory.setModbusRegisterValue("test",1,1,modmqttd::RegisterType::HOLDING, 5);

        auto reg = registers.addPoll(1, 1, std::chrono::milliseconds(5));
        executor.setupInitialPoll(registers);
        waitTime = executor.pollNext();
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        REQUIRE(reg->getValues()[0] == 5);
        REQUIRE(executor.allDone());
    }

    SECTION("after doing initial poll for single register") {
        modbus_factory.setModbusRegisterValue("test",1,1,modmqttd::RegisterType::HOLDING, 5);

        auto reg = registers.addPollDelayed(1, 1, std::chrono::milliseconds(50));
        executor.setupInitialPoll(registers);
        waitTime = executor.pollNext();
        REQUIRE(executor.allDone());

        SECTION("should delay register read on normal poll") {
            // need to wait because there is no silence between inital poll and
            // next poll
            executor.addPollList(registers);
            waitTime = executor.pollNext();
            REQUIRE(waitTime > std::chrono::milliseconds(40));
            REQUIRE(!executor.allDone());

            //simulate shorter wait than required
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            waitTime = executor.pollNext();
            REQUIRE(!executor.allDone());
            REQUIRE(std::chrono::milliseconds::zero() < waitTime);
            REQUIRE(waitTime < std::chrono::milliseconds(50));

            // required silence period reached
            std::this_thread::sleep_for(std::chrono::milliseconds(40));
            waitTime = executor.pollNext();
            REQUIRE(waitTime == std::chrono::milliseconds::zero());
            REQUIRE(reg->getValues()[0] == 5);
            REQUIRE(executor.allDone());
        }

        SECTION("should not delay register read if there was enough silence time") {
            std::this_thread::sleep_for(std::chrono::milliseconds(70));

            executor.addPollList(registers);
            waitTime = executor.pollNext();
            REQUIRE(waitTime == std::chrono::milliseconds::zero());
            REQUIRE(reg->getValues()[0] == 5);
            REQUIRE(executor.allDone());
        }
    }


    SECTION("after initial poll of two registers") {
        modbus_factory.setModbusRegisterValue("test",1,1,modmqttd::RegisterType::HOLDING, 1);
        modbus_factory.setModbusRegisterValue("test",2,20,modmqttd::RegisterType::HOLDING, 6);

        auto reg1 = registers.addPoll(1, 1);
        auto reg2 = registers.addPollDelayed(2, 20, std::chrono::milliseconds(50));

        executor.setupInitialPoll(registers);
        executor.pollNext(); // 2.20 is polled first because it requires silence
        REQUIRE(reg2->getValues()[0] == 6);
        executor.pollNext();
        REQUIRE(reg1->getValues()[0] == 1);
        REQUIRE(executor.allDone());

        modbus_factory.setModbusRegisterValue("test",1,1,modmqttd::RegisterType::HOLDING, 10);
        modbus_factory.setModbusRegisterValue("test",2,20,modmqttd::RegisterType::HOLDING, 60);

        SECTION("should poll waiting register with max delay first") {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            executor.addPollList(registers);
            waitTime = executor.pollNext();
            REQUIRE(waitTime == std::chrono::milliseconds::zero());
            REQUIRE(reg2->getValues()[0] == 60);
            REQUIRE(!executor.allDone());

            waitTime = executor.pollNext();
            REQUIRE(waitTime == std::chrono::milliseconds::zero());
            REQUIRE(reg1->getValues()[0] == 10);
            REQUIRE(executor.allDone());
        }
    }

    SECTION("after initial poll of three registers") {
        modbus_factory.setModbusRegisterValue("test",1,1,modmqttd::RegisterType::HOLDING, 1);
        modbus_factory.setModbusRegisterValue("test",2,20,modmqttd::RegisterType::HOLDING, 20);
        modbus_factory.setModbusRegisterValue("test",2,21,modmqttd::RegisterType::HOLDING, 21);

        auto reg1 = registers.addPoll(1, 1);
        auto reg2 = registers.addPoll(2, 20);
        auto reg3 = registers.addPollDelayed(2, 21, std::chrono::milliseconds(50));

        executor.setupInitialPoll(registers);
        executor.pollNext(); // 2.21 is polled first because it requires silence
        REQUIRE(reg3->getValues()[0] == 21);
        executor.pollNext(); // 2.20 is polled next because w group reads by slave
        REQUIRE(reg2->getValues()[0] == 20);
        executor.pollNext();
        REQUIRE(reg1->getValues()[0] == 1);
        REQUIRE(executor.allDone());

    }

    SECTION("should alternately poll and write to the same slave") {
        modbus_factory.setModbusRegisterValue("test",1,1,modmqttd::RegisterType::HOLDING, 1);
        modbus_factory.setModbusRegisterValue("test",1,20,modmqttd::RegisterType::HOLDING, 10);
        modbus_factory.setModbusRegisterValue("test",1,21,modmqttd::RegisterType::HOLDING, 20);
        registers.addPoll(1, 1);
        registers.addPoll(1, 10);
        registers.addPoll(1, 20);

        //mWaitingRegister is set to poll 1,1
        executor.setupInitialPoll(registers);
        executor.addWriteCommand(1, ModbusExecutorTestRegisters::createWrite(1, 100));
        executor.addWriteCommand(1, ModbusExecutorTestRegisters::createWrite(10, 101));
        executor.addWriteCommand(1, ModbusExecutorTestRegisters::createWrite(20, 102));
        executor.addWriteCommand(1, ModbusExecutorTestRegisters::createWrite(1, 200));
        executor.addWriteCommand(1, ModbusExecutorTestRegisters::createWrite(10, 201));
        executor.addWriteCommand(1, ModbusExecutorTestRegisters::createWrite(20, 202));

        REQUIRE(executor.getCommandsLeft() == 6);

        executor.pollNext(); //poll 1,1
        REQUIRE(fromModbusQueue.size_approx() == 1);
        executor.pollNext(); //write 1,1
        REQUIRE(modbus_factory.getModbusRegisterValue("test", 1, 1, modmqttd::RegisterType::HOLDING) == 100);

        executor.pollNext(); //poll 1,10
        REQUIRE(fromModbusQueue.size_approx() == 2);
        executor.pollNext(); //write 1,10
        REQUIRE(modbus_factory.getModbusRegisterValue("test", 1, 10, modmqttd::RegisterType::HOLDING) == 101);

        executor.pollNext(); //poll 1,20
        REQUIRE(fromModbusQueue.size_approx() == 3);
        executor.pollNext(); //write 1,20
        REQUIRE(modbus_factory.getModbusRegisterValue("test", 1, 20, modmqttd::RegisterType::HOLDING) == 102);

        // write only mode, start writing 20x values
        executor.pollNext(); //write 1,1
        REQUIRE(modbus_factory.getModbusRegisterValue("test", 1, 1, modmqttd::RegisterType::HOLDING) == 200);

        REQUIRE(executor.getCommandsLeft() == modmqttd::ModbusExecutor::WRITE_BATCH_SIZE - 1);

        executor.pollNext(); //write 1,10
        REQUIRE(modbus_factory.getModbusRegisterValue("test", 1, 10, modmqttd::RegisterType::HOLDING) == 201);

        //back to read/write mode, mWaiting register reset to 1,1
        executor.addPollList(registers);
        REQUIRE(executor.getCommandsLeft() == 6);

        executor.pollNext(); //poll 1,1
        REQUIRE(fromModbusQueue.size_approx() == 4);
        executor.pollNext(); //write 1,20
        REQUIRE(modbus_factory.getModbusRegisterValue("test", 1, 20, modmqttd::RegisterType::HOLDING) == 202);

        executor.pollNext(); //poll 1,10
        REQUIRE(fromModbusQueue.size_approx() == 5);
        executor.pollNext(); //poll 1,20
        REQUIRE(fromModbusQueue.size_approx() == 6);


        REQUIRE(executor.getCommandsLeft() == 2);
        REQUIRE(executor.allDone());
    }


    SECTION("should switch slaves after WRITE_BATCH_SIZE writes in write only mode") {
        modbus_factory.setModbusRegisterValue("test",1,1,modmqttd::RegisterType::HOLDING, 1);
        modbus_factory.setModbusRegisterValue("test",2,2,modmqttd::RegisterType::HOLDING, 20);

        for (int i = 1; i <= modmqttd::ModbusExecutor::WRITE_BATCH_SIZE+1; i++) {
            executor.addWriteCommand(1, ModbusExecutorTestRegisters::createWrite(1, i));
        }
        executor.addWriteCommand(2, ModbusExecutorTestRegisters::createWrite(2, 200));

        for (int i = 0; i < modmqttd::ModbusExecutor::WRITE_BATCH_SIZE; i++) {
            executor.pollNext(); //write to the first slave
        }

        REQUIRE(modbus_factory.getModbusRegisterValue("test", 1, 1, modmqttd::RegisterType::HOLDING) == modmqttd::ModbusExecutor::WRITE_BATCH_SIZE);
        REQUIRE(modbus_factory.getModbusRegisterValue("test", 2, 2, modmqttd::RegisterType::HOLDING) == 20);
        REQUIRE(executor.getCommandsLeft() == 0);

        executor.pollNext(); //write to the second slave

        REQUIRE(modbus_factory.getModbusRegisterValue("test", 1, 1, modmqttd::RegisterType::HOLDING) == modmqttd::ModbusExecutor::WRITE_BATCH_SIZE);
        REQUIRE(modbus_factory.getModbusRegisterValue("test", 2, 2, modmqttd::RegisterType::HOLDING) == 200);
        REQUIRE(executor.getCommandsLeft() == modmqttd::ModbusExecutor::WRITE_BATCH_SIZE-1);

    }
}
