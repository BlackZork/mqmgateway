#pragma once

#include "../readerwriterqueue/readerwriterqueue.h"

#include "common.hpp"
#include "queue_item.hpp"
#include "modbus_messages.hpp"
#include "modbus_scheduler.hpp"
#include "modbus_slave.hpp"
#include "imodbuscontext.hpp"

namespace modmqttd {

class ModbusThread {
    public:
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
        std::chrono::steady_clock::duration mMinDelayBeforePoll = std::chrono::steady_clock::duration::zero();

        // slave config
        std::map<int, ModbusSlaveConfig> mSlaves;

        //poll specs from slave config merged with mqtt section
        std::map<int, std::vector<std::shared_ptr<RegisterPoll>>> mRegisters;

        bool mShouldRun = true;
        // true if mqtt becomes online
        // and we need to refresh all registers
        bool mNeedInitialPoll = false;
        //true if mqtt is connected and we should
        //poll registers
        bool mShouldPoll = false;

        std::shared_ptr<IModbusContext> mModbus;
        ModbusScheduler mScheduler;

        void configure(const ModbusNetworkConfig& config);
        void setPollSpecification(const MsgRegisterPollSpecification& spec);
        void pollRegisters(int slaveId, const std::vector<std::shared_ptr<RegisterPoll>>& registers, bool sendIfChanged = true);
        void doInitialPoll();

        bool hasRegisters() const;

        void dispatchMessages(const QueueItem& read);
        void sendMessage(const QueueItem& item);

        void handleRegisterReadError(int slaveId, RegisterPoll& regPoll, const char* errorMessage);

        void processWrite(const MsgRegisterValues& msg);

        void processCommands();

        std::map<int, std::vector<std::shared_ptr<RegisterPoll>>> getListToRefresh(
            std::chrono::steady_clock::duration& outDuration,
            const std::chrono::time_point<std::chrono::steady_clock>& timePoint
        );
};

}
