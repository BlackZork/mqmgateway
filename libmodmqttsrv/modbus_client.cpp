#include "common.hpp"
#include "modbus_client.hpp"
#include "modbus_messages.hpp"
#include "threadutils.hpp"

namespace modmqttd {

void
ModbusClient::start(const ModbusNetworkConfig& config) {
    mNetworkName = config.mName;
    mThreadImpl.reset(new ModbusThread(config.mName, mToModbusQueue, mFromModbusQueue));
    mThread.reset(new std::thread(threadLoop, std::ref(*mThreadImpl)));
    mToModbusQueue.enqueue(QueueItem::create(config));
};

void ModbusClient::stop() {
    if (mThread != nullptr) {
        mToModbusQueue.enqueue(QueueItem::create(EndWorkMessage()));
        mThread->join();
        mThread.reset();
    }
};

void
ModbusClient::threadLoop(ModbusThread& threadImpl)
{
    threadImpl.run();
};

const ModbusThread&
ModbusClient::getThread() const {
    if (mThreadImpl == nullptr)
        throw std::logic_error("Modbus thread not created yet");
    return *mThreadImpl;
}


}; //namespace
