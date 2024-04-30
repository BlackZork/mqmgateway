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
        ModbusExecutor(moodycamel::BlockingReaderWriterQueue<QueueItem>& fromModbusQueue);
        void init(const std::shared_ptr<IModbusContext>& modbus) { mModbus = modbus; }
        void setupInitialPoll(const std::map<int, std::vector<std::shared_ptr<RegisterPoll>>>& pRegisters);
        bool allDone() const;
        bool pollDone() const;
        void addPollList(const std::map<int, std::vector<std::shared_ptr<RegisterPoll>>>& pRegisters, bool mInitialPoll = false);
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
        std::chrono::steady_clock::duration pollNext();

        std::chrono::steady_clock::duration getTotalPollDuration() const {
            return std::chrono::steady_clock::now() - mPollStart;
        }
        bool isInitial() const { return mInitialPoll; }
    private:
        boost::log::sources::severity_logger<Log::severity> log;

        std::shared_ptr<IModbusContext> mModbus;
        moodycamel::BlockingReaderWriterQueue<QueueItem>& mFromModbusQueue;


        std::map<int, ModbusRequestsQueues> mQueues;
        std::map<int, ModbusRequestsQueues>::iterator mCurrentQueue;

        // max number of requests to single slave after
        // we switch to next one. Used to avoid
        // execution starvation if setPollList
        // is called faster than executor is able to handle them.

        int mCommandsLeft = 1;
        int mBatchSize = 1;

        std::chrono::steady_clock::time_point mLastPollTime;

        //used to determine if we have to respect delay of RegisterPoll::ReadDelayType::FIRST_READ
        std::map<int, ModbusRequestsQueues>::iterator mLastQueue;
        std::shared_ptr<IRegisterCommand> mWaitingRegister;
        bool mInitialPoll;
        std::chrono::time_point<std::chrono::steady_clock> mPollStart;

        void sendCommand();
        void pollRegister(int slaveId, RegisterPoll& reg_ptr, bool forceSend);
        void sendMessage(const QueueItem& item);
        void handleRegisterReadError(int slaveId, RegisterPoll& reg, const char* errorMessage);
        void setBatchSize(int size);
};

}
