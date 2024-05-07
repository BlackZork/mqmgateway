#pragma once

#include "../readerwriterqueue/readerwriterqueue.h"

#include "common.hpp"
#include "register_poll.hpp"
#include "modbus_request_queues.hpp"
#include "modbus_context.hpp"
#include "queue_item.hpp"

namespace modmqttd {

class ModbusExecutor {
    public:
        static constexpr short WRITE_BATCH_SIZE = 10;

        ModbusExecutor(
            moodycamel::BlockingReaderWriterQueue<QueueItem>& fromModbusQueue,
            moodycamel::BlockingReaderWriterQueue<QueueItem>& toModbusQueue
        );
        void init(const std::shared_ptr<IModbusContext>& modbus) { mModbus = modbus; }
        void setupInitialPoll(const std::map<int, std::vector<std::shared_ptr<RegisterPoll>>>& pRegisters);
        bool allDone() const;
        bool pollDone() const;

        void addPollList(const std::map<int, std::vector<std::shared_ptr<RegisterPoll>>>& pRegisters, bool mInitialPoll = false);
        void addWriteCommand(int slaveId, const std::shared_ptr<RegisterWrite>& pCommand);
        /**
         *  Get next request R to send from modbus queues
         *  If R needs delay then return how much time we should wait before
         *  issuing modbus command
         *
         *  If R does not need delay then issue modbus command
         *  and return duration=0
         *
         * If queues are empty return duration=max
         */
        std::chrono::steady_clock::duration executeNext();

        bool isInitialPollInProgress() const { return mInitialPoll; }

        int getCommandsLeft() const { return mCommandsLeft; }

        const std::shared_ptr<IRegisterCommand>& getWaitingCommand() const { return mWaitingCommand; }
        /*
            Returns last command executed by executeNext or nullptr if executeNext()
            returns non-zero duration
        */
        const std::shared_ptr<IRegisterCommand>& getLastCommand() const { return mLastCommand; }

        void setMaxReadRetryCount(short val) { mMaxReadRetryCount = mReadRetryCount = val; }
        void setMaxWriteRetryCount(short val) { mMaxWriteRetryCount = mWriteRetryCount = val; }
    private:
        static  boost::log::sources::severity_logger<Log::severity> log;

        std::shared_ptr<IModbusContext> mModbus;
        moodycamel::BlockingReaderWriterQueue<QueueItem>& mFromModbusQueue;
        moodycamel::BlockingReaderWriterQueue<QueueItem>& mToModbusQueue;

        std::map<int, ModbusRequestsQueues> mSlaveQueues;
        std::map<int, ModbusRequestsQueues>::iterator mCurrentSlaveQueue;

        // max number of requests to single slave after
        // we switch to next one. Used to avoid
        // execution starvation if addWriteCommand
        // is called faster than executor is able to handle them.
        int mCommandsLeft = 0;

        short mMaxReadRetryCount = 0;
        short mMaxWriteRetryCount = 3;
        short mWriteRetryCount;
        short mReadRetryCount;

        std::chrono::steady_clock::time_point mLastCommandTime;

        //used to determine if we have to respect delay of RegisterPoll::ReadDelayType::ON_SLAVE_CHANGE
        std::map<int, ModbusRequestsQueues>::iterator mLastQueue;
        std::shared_ptr<IRegisterCommand> mWaitingCommand;
        std::shared_ptr<IRegisterCommand> mLastCommand;

        bool mInitialPoll;
        std::chrono::time_point<std::chrono::steady_clock> mInitialPollStart;

        void sendCommand();
        void pollRegisters(int slaveId, RegisterPoll& reg_ptr, bool forceSend);
        void writeRegisters(int slaveId, RegisterWrite& cmd);
        void sendMessage(const QueueItem& item);
        void handleRegisterReadError(int slaveId, RegisterPoll& reg, const char* errorMessage);
        void resetCommandsCounter();
};

}
