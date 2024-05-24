#pragma once

#include <thread>
#include "queue_item.hpp"
#include "mqttobject.hpp"
#include "mqttcommand.hpp"
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

        void sendCommand(const MqttObjectCommand& cmd, const ModbusRegisters& reg_values) {
            MsgRegisterValues val(
                cmd.mSlaveId,
                cmd.mRegisterType,
                cmd.mRegister,
                reg_values,
                cmd.getCommandId()
            );
            // TODO add max queue size
            // here or at mqtt level - add configurable global limit for all queues
            // to i.e. 15Mb and cut the largest one after reaching this limit
            mToModbusQueue.enqueue(QueueItem::create(val));
        }

        void sendMqttNetworkIsUp(bool up) {
            // TODO send all control messages at the front of queue, add time period
            // after receiving shutdown request to empty write queues
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
