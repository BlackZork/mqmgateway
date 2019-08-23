#include "common.hpp"
#include "modbus_client.hpp"
#include "modbus_thread.hpp"
#include "modbus_messages.hpp"

namespace modmqttd {

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
