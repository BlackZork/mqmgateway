#include "common.hpp"
#include "modbus_client.hpp"
#include "modbus_thread.hpp"
#include "modbus_messages.hpp"
#include "threadutils.hpp"

namespace modmqttd {

void
ModbusClient::init(const ModbusNetworkConfig& config) {
    mNetworkName = config.mName;
    mModbusThread.reset(new std::thread(threadLoop, std::ref(mNetworkName), std::ref(mToModbusQueue), std::ref(mFromModbusQueue)));
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
    const std::string& pNetworkName,
    moodycamel::BlockingReaderWriterQueue<QueueItem>& toModbusQueue,
    moodycamel::BlockingReaderWriterQueue<QueueItem>& fromModbusQueue)
{
    ThreadUtils::set_thread_name(pNetworkName.c_str());
    ModbusThread thread(pNetworkName, toModbusQueue, fromModbusQueue);
    thread.run();
};

}; //namespace
