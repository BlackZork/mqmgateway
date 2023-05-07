#pragma once

#include <thread>
#include "queue_item.hpp"
#include "mqttobject.hpp"
#include "modbus_messages.hpp"
#include "../readerwriterqueue/readerwriterqueue.h"

namespace modmqttd {
/**
 * This class contains code executed in main thread context
 * Rest is in ModbusThread class.
 * */
class ModbusClient {
    public:
        ModbusClient() {};
        moodycamel::BlockingReaderWriterQueue<QueueItem> mFromModbusQueue;
        moodycamel::BlockingReaderWriterQueue<QueueItem> mToModbusQueue;

        void init(const ModbusNetworkConfig& config) {
            mName = config.mName;
            mModbusThread.reset(new std::thread(threadLoop, std::ref(mToModbusQueue), std::ref(mFromModbusQueue)));
            mToModbusQueue.enqueue(QueueItem::create(config));
        };

        void sendCommand(const MqttObjectBase& cmd, uint16_t value) {
            MsgRegisterValue val(
                cmd.mRegister.mSlaveId,
                cmd.mRegister.mRegisterType,
                cmd.mRegister.mRegisterAddress,
                value
            );
            mToModbusQueue.enqueue(QueueItem::create(val));
        }

        void sendMqttNetworkIsUp(bool up) {
            mToModbusQueue.enqueue(QueueItem::create(MsgMqttNetworkState(up)));
        }

        std::string mName;

        void stop();
        ~ModbusClient() { stop(); }
    private:
        static void threadLoop(moodycamel::BlockingReaderWriterQueue<QueueItem>& in, moodycamel::BlockingReaderWriterQueue<QueueItem>& out);

        ModbusClient(const ModbusClient&);
        std::shared_ptr<std::thread> mModbusThread;
};


}
