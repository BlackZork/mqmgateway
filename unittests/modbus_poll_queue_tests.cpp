
#include "catch2/catch_all.hpp"

#include "libmodmqttsrv/modbus_poll_queue.hpp"
#include "libmodmqttsrv/register_poll.hpp"

#include "modbus_utils.hpp"


TEST_CASE("ModbusPollQueue") {
    modmqttd::ModbusPollQueue queue;
    ModbusExecutorTestRegisters registers;

    SECTION("should move all elements from waiting list to poll queue on popNext") {
        registers.add(1,1);
        queue.addPollList(registers[1]);

        REQUIRE(queue.mWaitingList.size() == 1);
        REQUIRE(queue.mPollQueue.size() == 0);
        REQUIRE(!queue.empty());
        std::shared_ptr<modmqttd::RegisterPoll> reg1(queue.popNext());
        REQUIRE(reg1 == registers[1].front());
        REQUIRE(queue.empty());
    }

    SECTION("should not insert duplicate item to waiting list") {
        registers.add(1,1);
        queue.addPollList(registers[1]);
        queue.addPollList(registers[1]);

        REQUIRE(queue.mWaitingList.size() == 1);
    }

    SECTION("should not add elements to waiting list if they are queued") {
        registers.add(1,1);
        registers.add(1,2);

        queue.addPollList(registers[1]);
        std::shared_ptr<modmqttd::RegisterPoll> reg1(queue.popNext());

        registers[1].clear();
        registers.add(1,1);

        REQUIRE(queue.mWaitingList.size() == 0);
        REQUIRE(queue.mPollQueue.size() == 1);
    }

    SECTION("should add elements to waiting list if they are not queued") {
        registers.add(1,1);
        registers.add(1,2);
        registers.add(1,3);

        queue.addPollList(registers[1]);
        std::shared_ptr<modmqttd::RegisterPoll> reg1(queue.popNext());

        registers[1].clear();

        registers.add(1,4);
        queue.addPollList(registers[1]);

        std::shared_ptr<modmqttd::RegisterPoll> reg2(queue.popNext());
        REQUIRE(queue.mPollQueue.size() == 1);
        REQUIRE(queue.mWaitingList.size() == 1);
    }
}
