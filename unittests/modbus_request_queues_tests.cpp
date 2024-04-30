
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

    SECTION("should return best fit for delayed register") {
        registers.addDelayed(1,1, std::chrono::milliseconds(50));
        registers.addDelayed(1,2, std::chrono::milliseconds(100));
        registers.add(1,3);

        queue.addPollList(registers[1]);

        auto dur = queue.findForSilencePeriod(std::chrono::milliseconds(100), true);
        REQUIRE(dur == std::chrono::milliseconds(100));

        std::shared_ptr<modmqttd::IRegisterCommand> reg = queue.popFirstWithDelay(std::chrono::milliseconds(100), true);
        REQUIRE(reg->getRegister() == 1);

        dur = queue.findForSilencePeriod(std::chrono::milliseconds(100), false);
        REQUIRE(dur == std::chrono::milliseconds(50));

    }

    SECTION("should return best fit for delayed register ignoring first_request delays") {
        registers.addDelayed(1,1, std::chrono::milliseconds(50));
        registers.addDelayed(1,2, std::chrono::milliseconds(100), modmqttd::ModbusCommandDelay::DelayType::ON_SLAVE_CHANGE);
        registers.add(1,3);

        queue.addPollList(registers[1]);

        auto dur = queue.findForSilencePeriod(std::chrono::milliseconds(100), true);
        REQUIRE(dur == std::chrono::milliseconds(50));

        dur = queue.findForSilencePeriod(std::chrono::milliseconds(100), false);
        REQUIRE(dur == std::chrono::milliseconds(100));

    }


}
