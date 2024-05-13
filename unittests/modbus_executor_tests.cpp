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

        waitTime = executor.executeNext();
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        REQUIRE(executor.allDone());

        executor.addPollList(registers);
        waitTime = executor.executeNext();
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        REQUIRE(executor.allDone());
    }

    SECTION("should immediately do initial poll for single register") {
        modbus_factory.setModbusRegisterValue("test",1,1,modmqttd::RegisterType::HOLDING, 5);

        auto reg = registers.addPoll(1, 1);
        executor.setupInitialPoll(registers);
        waitTime = executor.executeNext();
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
        waitTime = executor.executeNext();
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        REQUIRE(reg1->getValues()[0] == 5);
        REQUIRE(!executor.allDone());

        waitTime = executor.executeNext();
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
        waitTime = executor.executeNext();
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        REQUIRE(reg1->getValues()[0] == 5);
        REQUIRE(!executor.allDone());

        waitTime = executor.executeNext();
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        REQUIRE(reg2->getValues()[0] == 60);
        REQUIRE(executor.allDone());
    }

    SECTION("should not delay register read on initial poll") {
        modbus_factory.setModbusRegisterValue("test",1,1,modmqttd::RegisterType::HOLDING, 5);

        auto reg = registers.addPoll(1, 1, std::chrono::milliseconds(5));
        executor.setupInitialPoll(registers);
        waitTime = executor.executeNext();
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        REQUIRE(reg->getValues()[0] == 5);
        REQUIRE(executor.allDone());
    }

    SECTION("after doing initial poll for single register") {
        modbus_factory.setModbusRegisterValue("test",1,1,modmqttd::RegisterType::HOLDING, 5);

        auto reg = registers.addPollDelayed(1, 1, std::chrono::milliseconds(50));
        executor.setupInitialPoll(registers);
        waitTime = executor.executeNext();
        REQUIRE(executor.allDone());

        SECTION("should delay register read on normal poll") {
            // need to wait because there is no silence between inital poll and
            // next poll
            executor.addPollList(registers);
            waitTime = executor.executeNext();
            REQUIRE(waitTime > std::chrono::milliseconds(40));
            REQUIRE(!executor.allDone());

            //simulate shorter wait than required
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            waitTime = executor.executeNext();
            REQUIRE(!executor.allDone());
            REQUIRE(std::chrono::milliseconds::zero() < waitTime);
            REQUIRE(waitTime < std::chrono::milliseconds(50));

            // required silence period reached
            std::this_thread::sleep_for(std::chrono::milliseconds(40));
            waitTime = executor.executeNext();
            REQUIRE(waitTime == std::chrono::milliseconds::zero());
            REQUIRE(reg->getValues()[0] == 5);
            REQUIRE(executor.allDone());
        }

        SECTION("should not delay register read if there was enough silence time") {
            std::this_thread::sleep_for(std::chrono::milliseconds(70));

            executor.addPollList(registers);
            waitTime = executor.executeNext();
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
        executor.executeNext(); // 2.20 is polled first because it requires silence
        REQUIRE(reg2->getValues()[0] == 6);
        executor.executeNext();
        REQUIRE(reg1->getValues()[0] == 1);
        REQUIRE(executor.allDone());

        modbus_factory.setModbusRegisterValue("test",1,1,modmqttd::RegisterType::HOLDING, 10);
        modbus_factory.setModbusRegisterValue("test",2,20,modmqttd::RegisterType::HOLDING, 60);

        SECTION("should poll waiting register with max delay first") {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            executor.addPollList(registers);
            waitTime = executor.executeNext();
            REQUIRE(waitTime == std::chrono::milliseconds::zero());
            REQUIRE(reg2->getValues()[0] == 60);
            REQUIRE(!executor.allDone());

            waitTime = executor.executeNext();
            REQUIRE(waitTime == std::chrono::milliseconds::zero());
            REQUIRE(reg1->getValues()[0] == 10);
            REQUIRE(executor.allDone());
        }
    }

    SECTION("should poll slave that was polled before due to election with biggest delay") {
        modbus_factory.setModbusRegisterValue("test",1,1,modmqttd::RegisterType::HOLDING, 1);
        modbus_factory.setModbusRegisterValue("test",2,20,modmqttd::RegisterType::HOLDING, 20);
        modbus_factory.setModbusRegisterValue("test",2,21,modmqttd::RegisterType::HOLDING, 21);

        auto reg1 = registers.addPoll(1, 1);
        auto reg2 = registers.addPoll(2, 20);
        auto reg3 = registers.addPollDelayed(2, 21, std::chrono::milliseconds(50));

        executor.setupInitialPoll(registers);
        executor.executeNext(); // 2.21 is polled first because it requires silence
        REQUIRE(reg3->getValues()[0] == 21);
        executor.executeNext(); // 2.20 is polled next because we group reads by slave
        REQUIRE(reg2->getValues()[0] == 20);
        executor.executeNext();
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

        //mWaitingCommand is set to poll 1,1
        executor.setupInitialPoll(registers);
        executor.addWriteCommand(ModbusExecutorTestRegisters::createWrite(1, 1, 100));
        executor.addWriteCommand(ModbusExecutorTestRegisters::createWrite(1, 10, 101));
        executor.addWriteCommand(ModbusExecutorTestRegisters::createWrite(1, 20, 102));
        executor.addWriteCommand(ModbusExecutorTestRegisters::createWrite(1, 1, 200));
        executor.addWriteCommand(ModbusExecutorTestRegisters::createWrite(1, 10, 201));
        executor.addWriteCommand(ModbusExecutorTestRegisters::createWrite(1, 20, 202));

        REQUIRE(executor.getCommandsLeft() == 6);

        //first write prioirty kicks in
        executor.executeNext(); //write 1,1
        REQUIRE(modbus_factory.getModbusRegisterValue("test", 1, 1, modmqttd::RegisterType::HOLDING) == 100);
        executor.executeNext(); //poll 1,1
        REQUIRE(fromModbusQueue.size_approx() == 1);

        executor.executeNext(); //write 1,10
        REQUIRE(modbus_factory.getModbusRegisterValue("test", 1, 10, modmqttd::RegisterType::HOLDING) == 101);
        executor.executeNext(); //poll 1,10
        REQUIRE(fromModbusQueue.size_approx() == 2);

        executor.executeNext(); //write 1,20
        REQUIRE(modbus_factory.getModbusRegisterValue("test", 1, 20, modmqttd::RegisterType::HOLDING) == 102);
        executor.executeNext(); //poll 1,20
        REQUIRE(fromModbusQueue.size_approx() == 3);

        // write only mode, start writing 20x values
        executor.executeNext(); //write 1,1
        REQUIRE(modbus_factory.getModbusRegisterValue("test", 1, 1, modmqttd::RegisterType::HOLDING) == 200);

        REQUIRE(executor.getCommandsLeft() == modmqttd::ModbusExecutor::WRITE_BATCH_SIZE - 1);

        executor.executeNext(); //write 1,10
        REQUIRE(modbus_factory.getModbusRegisterValue("test", 1, 10, modmqttd::RegisterType::HOLDING) == 201);

        //back to read/write mode, queues are not setup because we still have something to write
        //now we have more commands that poll size due to WRITE_BATCH_SIZE set in write only period
        //TODO maybe allow to set commandsLeft to lower value to align with new poll size?
        executor.addPollList(registers);
        REQUIRE(executor.getCommandsLeft() == 8);

        executor.executeNext(); //poll 1,1
        REQUIRE(fromModbusQueue.size_approx() == 4);
        executor.executeNext(); //write 1,20
        REQUIRE(modbus_factory.getModbusRegisterValue("test", 1, 20, modmqttd::RegisterType::HOLDING) == 202);

        executor.executeNext(); //poll 1,10
        REQUIRE(fromModbusQueue.size_approx() == 5);
        executor.executeNext(); //poll 1,20
        REQUIRE(fromModbusQueue.size_approx() == 6);


        REQUIRE(executor.getCommandsLeft() == 4);
        REQUIRE(executor.allDone());
    }


    SECTION("should switch slaves after WRITE_BATCH_SIZE writes in write only mode") {
        modbus_factory.setModbusRegisterValue("test",1,1,modmqttd::RegisterType::HOLDING, 1);
        modbus_factory.setModbusRegisterValue("test",2,2,modmqttd::RegisterType::HOLDING, 20);

        for (int i = 1; i <= modmqttd::ModbusExecutor::WRITE_BATCH_SIZE+1; i++) {
            executor.addWriteCommand(ModbusExecutorTestRegisters::createWrite(1, 1, i));
        }
        executor.addWriteCommand(ModbusExecutorTestRegisters::createWrite(2, 2, 200));

        for (int i = 0; i < modmqttd::ModbusExecutor::WRITE_BATCH_SIZE; i++) {
            executor.executeNext(); //write to the first slave
        }

        REQUIRE(modbus_factory.getModbusRegisterValue("test", 1, 1, modmqttd::RegisterType::HOLDING) == modmqttd::ModbusExecutor::WRITE_BATCH_SIZE);
        REQUIRE(modbus_factory.getModbusRegisterValue("test", 2, 2, modmqttd::RegisterType::HOLDING) == 20);
        REQUIRE(executor.getCommandsLeft() == 0);

        executor.executeNext(); //switch and write to the second slave

        REQUIRE(modbus_factory.getModbusRegisterValue("test", 1, 1, modmqttd::RegisterType::HOLDING) == modmqttd::ModbusExecutor::WRITE_BATCH_SIZE);
        REQUIRE(modbus_factory.getModbusRegisterValue("test", 2, 2, modmqttd::RegisterType::HOLDING) == 200);
        REQUIRE(executor.getCommandsLeft() == modmqttd::ModbusExecutor::WRITE_BATCH_SIZE-1);

    }

    SECTION("should not reelect mWaitingCommand poll is not finished") {
        modbus_factory.setModbusRegisterValue("test",1,1,modmqttd::RegisterType::HOLDING, 1);
        modbus_factory.setModbusRegisterValue("test",1,20,modmqttd::RegisterType::HOLDING, 10);
        modbus_factory.setModbusRegisterValue("test",1,21,modmqttd::RegisterType::HOLDING, 20);
        registers.addPoll(1, 1);
        registers.addPollDelayed(1, 2, std::chrono::milliseconds(50));
        registers.addPoll(1, 3);

        // do initial poll
        executor.setupInitialPoll(registers);
        REQUIRE(executor.getCommandsLeft() == 6);
        executor.executeNext(); //1,2
        executor.executeNext(); //1,1 or 1,3
        executor.executeNext(); //1,1 or 1,3

        // elect 1.2 after enough silence
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        executor.addPollList(registers);
        REQUIRE(executor.getCommandsLeft() == 6);
        REQUIRE(executor.getWaitingCommand()->getRegister() == 1);
        executor.executeNext(); //1,1

        // still polling, should not relect 1,2
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        executor.addPollList(registers);
        //we do not increase commands left to avoid queue lock if
        //scheduler adds registers quickly
        REQUIRE(executor.getCommandsLeft() == 5);
        REQUIRE(executor.getWaitingCommand() == nullptr);
    }

    SECTION("should set next queue to added poll item if queues are empty") {

        registers.addPoll(1, 1);
        registers.addPoll(2, 2);
        registers.addPoll(3, 3);

        executor.setupInitialPoll(registers);
        executor.executeNext();
        executor.executeNext();
        executor.executeNext();
        REQUIRE(executor.allDone());

        registers.clear();
        registers.addPoll(4,4);
        executor.addPollList(registers);
        executor.executeNext();
        REQUIRE(executor.getLastCommand()->getRegister() == 3);
    }


    SECTION("should retry last write command mMaxWriteRetryCount times if failed") {
        modbus_factory.setModbusRegisterWriteError("test", 1, 1, modmqttd::RegisterType::HOLDING);

        auto cmd(registers.createWrite(1, 1, 0x3));
        cmd->setMaxRetryCounts(0,1);
        executor.addWriteCommand(cmd);

        executor.executeNext();
        REQUIRE(!executor.allDone());
        REQUIRE(executor.getWaitingCommand()->getRegister() == 0);

        executor.executeNext();
        REQUIRE(executor.allDone());
        REQUIRE(executor.getWaitingCommand() == nullptr);
        REQUIRE(executor.getLastCommand()->executedOk() == false);
    }

    SECTION("should delay retry of last write command") {
        modbus_factory.setModbusRegisterWriteError("test", 1, 1, modmqttd::RegisterType::HOLDING);

        auto cmd(registers.createWriteDelayed(1, 1, 0x3, std::chrono::milliseconds(10)));
        cmd->setMaxRetryCounts(0,1);
        executor.addWriteCommand(cmd);

        executor.executeNext();
        REQUIRE(!executor.allDone());
        REQUIRE(executor.getWaitingCommand()->getRegister() == 0);

        auto delay = executor.executeNext();
        REQUIRE(!executor.allDone());
        REQUIRE(delay > std::chrono::milliseconds(5));

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        executor.executeNext();
        REQUIRE(executor.allDone());
        REQUIRE(executor.getWaitingCommand() == nullptr);
        REQUIRE(executor.getLastCommand()->executedOk() == false);
    }

}
