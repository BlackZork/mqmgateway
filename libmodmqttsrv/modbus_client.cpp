#include "common.hpp"
#include "modbus_client.hpp"
#include "modbus_thread.hpp"
#include "modbus_messages.hpp"
#include "threadutils.hpp"

namespace modmqttd {

void
ModbusClient::init(const ModbusNetworkConfig& config) {
    mNetworkName = config.mName;
    ThreadUtils::set_thread_name(mNetworkName.c_str());
    mModbusThread.reset(new std::thread(threadLoop, std::ref(mToModbusQueue), std::ref(mFromModbusQueue)));
    mToModbusQueue.enqueue(QueueItem::create(config));
};

void ModbusClient::stop() {
    if (mModbusThread != nullptr) {
        mToModbusQueue.enqueue(QueueItem::create(EndWorkMessage()));
        mModbusThread->join();
        mModbusThread.reset();
    }
};

void
ModbusClient::threadLoop(
    moodycamel::BlockingReaderWriterQueue<QueueItem>& toModbusQueue,
    moodycamel::BlockingReaderWriterQueue<QueueItem>& fromModbusQueue)
{
    ModbusThread thread(toModbusQueue, fromModbusQueue);
    thread.run();
};

}; //namespace
