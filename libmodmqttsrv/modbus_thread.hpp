#pragma once

#include "../readerwriterqueue/readerwriterqueue.h"

#include "common.hpp"
#include "modbus_messages.hpp"
#include "modbus_scheduler.hpp"
#include "modbus_slave.hpp"
#include "modbus_executor.hpp"
#include "modbus_watchdog.hpp"

#include "imodbuscontext.hpp"

namespace modmqttd {

class ModbusThread {
    public:
        static void sendMessageFromModbus(moodycamel::BlockingReaderWriterQueue<QueueItem>& fromModbusQueue, const QueueItem& item);

        ModbusThread(
            moodycamel::BlockingReaderWriterQueue<QueueItem>& toModbusQueue,
            moodycamel::BlockingReaderWriterQueue<QueueItem>& fromModbusQueue);
        void run();
    private:
        moodycamel::BlockingReaderWriterQueue<QueueItem>& mToModbusQueue;
        moodycamel::BlockingReaderWriterQueue<QueueItem>& mFromModbusQueue;

        // global config
        std::string mNetworkName;
        std::shared_ptr<std::chrono::milliseconds> mDelayBeforeCommand;
        std::shared_ptr<std::chrono::milliseconds> mDelayBeforeFirstCommand;
        short mMaxReadRetryCount;
        short mMaxWriteRetryCount;

        // slave config
        std::map<int, ModbusSlaveConfig> mSlaves;

        bool mShouldRun = true;

        bool mMqttConnected = false;
        bool mGotRegisters = false;

        std::shared_ptr<IModbusContext> mModbus;
        ModbusScheduler mScheduler;
        ModbusExecutor mExecutor;
        ModbusWatchdog mWatchdog;

        void configure(const ModbusNetworkConfig& config);
        void setPollSpecification(const MsgRegisterPollSpecification& spec);
        void updateFromSlaveConfig(const ModbusSlaveConfig& pSlaveConfig);

        void dispatchMessages(const QueueItem& read);
        void sendMessage(const QueueItem& item);

        void processWrite(const std::shared_ptr<MsgRegisterValues>& msg);

        void processCommands();
};

}
