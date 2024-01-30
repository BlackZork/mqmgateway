#pragma once

#include "../readerwriterqueue/readerwriterqueue.h"

#include "common.hpp"
#include "register_poll.hpp"
#include "modbus_context.hpp"
#include "queue_item.hpp"

namespace modmqttd {

class ModbusPoller {
    public:
        ModbusPoller(moodycamel::BlockingReaderWriterQueue<QueueItem>& fromModbusQueue);
        void init(const std::shared_ptr<IModbusContext>& modbus) { mModbus = modbus; }
        void setupInitialPoll(const std::map<int, std::vector<std::shared_ptr<RegisterPoll>>>& pRegisters);
        bool allDone() const { return mRegisters.empty(); }
        void setPollList(const std::map<int, std::vector<std::shared_ptr<RegisterPoll>>>& pRegisters, bool mInitialPoll = false);
        /**
         *  Get next register R to pull from register list
         *  If R needs delay then return how much time we should wait before
         *  issuing modbus read call
         *
         *  If R does not need delay then issue modbus read call
         *  and return duration=0
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
        std::map<int, std::vector<std::shared_ptr<RegisterPoll>>> mRegisters;

        std::chrono::time_point<std::chrono::steady_clock> mLastPollTime;

        //state
        int mCurrentSlave;
        //used to determine if we have to respect delay of RegisterPoll::ReadDelayType::FIRST_READ
        int mLastSlave;
        std::shared_ptr<RegisterPoll> mWaitingRegister;
        bool mInitialPoll;
        std::chrono::time_point<std::chrono::steady_clock> mPollStart;

        void pollRegister(int slaveId, const std::shared_ptr<RegisterPoll>& reg_ptr, bool forceSend);
        void sendMessage(const QueueItem& item);
        void handleRegisterReadError(int slaveId, RegisterPoll& regPoll, const char* errorMessage);
};

}
