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
        void add(
            int mSlave,
            modmqttd::RegisterType type,
            int mNumber,
            std::chrono::steady_clock::duration mRefresh = std::chrono::milliseconds(10),
            std::chrono::steady_clock::duration mDelayBeforePoll = std::chrono::milliseconds::zero()
        );
};


TEST_CASE("ModbusPoller") {
    moodycamel::BlockingReaderWriterQueue<modmqttd::QueueItem> fromModbusQueue;
    MockedModbusFactory modbus_factory;

    modmqttd::ModbusPoller poller(fromModbusQueue);
    poller.init(modbus_factory.getContext("test"));

    SECTION("shoud return zero duration for empty register set") {
        std::map<int, std::vector<std::shared_ptr<modmqttd::RegisterPoll>>> registers;
        poller.setupInitialPoll(registers);

        auto waitTime = poller.pollNext();
        REQUIRE(waitTime == std::chrono::milliseconds::zero());
    }

}
