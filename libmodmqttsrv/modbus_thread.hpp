#pragma once

#include "../readerwriterqueue/readerwriterqueue.h"

#include "common.hpp"
#include "modbus_messages.hpp"
#include "modbus_scheduler.hpp"
#include "modbus_slave.hpp"
#include "modbus_executor.hpp"

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
        boost::log::sources::severity_logger<Log::severity> log;
        moodycamel::BlockingReaderWriterQueue<QueueItem>& mToModbusQueue;
        moodycamel::BlockingReaderWriterQueue<QueueItem>& mFromModbusQueue;

        // global config
        std::string mNetworkName;
        std::chrono::steady_clock::duration mDelayBeforeCommand = std::chrono::steady_clock::duration::zero();
        std::chrono::steady_clock::duration mDelayBeforeFirstCommand = std::chrono::steady_clock::duration::zero();

        // slave config
        std::map<int, ModbusSlaveConfig> mSlaves;

        bool mShouldRun = true;

        bool mMqttConnected = false;
        bool mGotRegisters = false;

        std::shared_ptr<IModbusContext> mModbus;
        ModbusScheduler mScheduler;
        ModbusExecutor mExecutor;

        void configure(const ModbusNetworkConfig& config);
        void setPollSpecification(const MsgRegisterPollSpecification& spec);
        void updateFromSlaveConfig(const ModbusSlaveConfig& pSlaveConfig);

        void dispatchMessages(const QueueItem& read);
        void sendMessage(const QueueItem& item);

        void processWrite(const std::shared_ptr<MsgRegisterValues>& msg);

        void processCommands();
};

}
