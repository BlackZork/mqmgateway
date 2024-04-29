
#include "catch2/catch_all.hpp"

#include "libmodmqttsrv/modbus_request_queues.hpp"
#include "libmodmqttsrv/register_poll.hpp"

#include "modbus_utils.hpp"


TEST_CASE("ModbusRequestQueues") {
    modmqttd::ModbusRequestsQueues queue;
    ModbusExecutorTestRegisters registers;

    SECTION("should merge poll list with elements already in queue") {
        registers.add(1,1);
        queue.addPollList(registers[1]);
        queue.addPollList(registers[1]);

        REQUIRE(queue.mPollQueue.size() == 1);
    }

    SECTION("should add elements to waiting list if they are not queued") {
        registers.add(1,1);

        queue.addPollList(registers[1]);

        registers[1].clear();
        registers.add(1,2);
        registers.add(1,3);

        queue.addPollList(registers[1]);
        REQUIRE(queue.mPollQueue.size() == 3);
    }
}
