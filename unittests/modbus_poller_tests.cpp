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
            std::chrono::milliseconds refresh = std::chrono::milliseconds(10),
            std::chrono::steady_clock::duration delayBeforePoll = std::chrono::milliseconds::zero()
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


        poller.setPollList(registers);
        waitTime = poller.pollNext();
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
    }

    SECTION("should immediately do initial poll for single register") {
        modbus_factory.setModbusRegisterValue("test",1,1,modmqttd::RegisterType::HOLDING, 5);

        auto reg = registers.add(1, 1);
        poller.setupInitialPoll(registers);
        auto waitTime = poller.pollNext();
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
        REQUIRE(reg->getValues()[0] == 5);
    }

}
