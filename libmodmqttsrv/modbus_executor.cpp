#include "modbus_executor.hpp"
#include "modbus_messages.hpp"
#include "modbus_thread.hpp"
#include "modbus_types.hpp"
#include "queue_item.hpp"

namespace modmqttd {

ModbusExecutor::ModbusExecutor(moodycamel::BlockingReaderWriterQueue<QueueItem>& fromModbusQueue)
    : mFromModbusQueue(fromModbusQueue)
{
    //some random past value, not using steady_clock:min() due to overflow
    mLastPollTime = std::chrono::steady_clock::now() - std::chrono::hours(100000);
    mLastQueue = mQueues.end();
    mCurrentQueue = mQueues.end();
}

void
ModbusExecutor::sendMessage(const QueueItem& item) {
    ModbusThread::sendMessageFromModbus(mFromModbusQueue, item);
}

void
ModbusExecutor::setupInitialPoll(const std::map<int, std::vector<std::shared_ptr<RegisterPoll>>>& pRegisters) {
    addPollList(pRegisters, true);
    BOOST_LOG_SEV(log, Log::debug) << "starting initial poll";
}


void
ModbusExecutor::addPollList(const std::map<int, std::vector<std::shared_ptr<RegisterPoll>>>& pRegisters, bool initialPoll) {

    bool setupQueues = allDone();

    mInitialPoll = initialPoll;
    if (mInitialPoll) {
        mPollStart = std::chrono::steady_clock::now();
        if (!pollDone()) {
            BOOST_LOG_SEV(log, Log::error) << "Cannot add next registers before initial poll is finished. Fix control loop.";
            return;
        }
    }


    bool hasRegisters = false;
    for (auto& pit: pRegisters) {
        auto& queue = mQueues[pit.first];
        queue.addPollList(pit.second);
        if (!queue.empty())
            hasRegisters = true;
    }

    // we are already sending requests or have nothing to do
    // so do not try to find what to read or write next
    if (!setupQueues || !hasRegisters) {
        return;
    }

    mCurrentQueue = mQueues.begin();

    // scan register list for registers that have delay_before_poll set
    // and find the best one that fits in the last_silence_period
    auto last_silence_period = std::chrono::steady_clock::now() - mLastPollTime;

    mWaitingRegister = nullptr;
    u_int16_t maxQueueSize = 0;

    auto currentDiff = std::chrono::steady_clock::duration::max();

    for(auto sit = mQueues.begin(); sit != mQueues.end(); sit++) {
        if (sit->second.mPollQueue.size() > maxQueueSize)
            maxQueueSize = sit->second.mPollQueue.size();

        bool ignore_first_read = (sit == mCurrentQueue);

        ModbusRequestDelay reg_delay = sit->second.findForSilencePeriod(last_silence_period, ignore_first_read);

        if (reg_delay < currentDiff) {
            std::shared_ptr<IRegisterCommand> reg(sit->second.popFirstWithDelay(last_silence_period, ignore_first_read));
            mWaitingRegister = reg;
            mCurrentQueue = sit;
            currentDiff = reg_delay;
            // we cannot break here even if currentDiff == 0
            // becaue maxQueueSize may be set incorrectly
        }
    }

    //if there are no registers with delay set start from the first queue
    if (currentDiff == std::chrono::steady_clock::duration::max()) {
        mCurrentQueue = mQueues.begin();
        mWaitingRegister = mCurrentQueue->second.popNext();
    }

    setBatchSize(maxQueueSize);
}


void
ModbusExecutor::pollRegister(int slaveId, RegisterPoll& reg, bool forceSend) {
    try {
        std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        std::vector<uint16_t> newValues(mModbus->readModbusRegisters(slaveId, reg));
        mLastPollTime = reg.mLastRead = std::chrono::steady_clock::now();

        std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
        BOOST_LOG_SEV(log, Log::debug) << "Register " << slaveId << "." << reg.mRegister << " (0x" << std::hex << slaveId << ".0x" << std::hex << reg.mRegister << ")"
                        << " polled in "  << std::dec << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms";

        if ((reg.getValues() != newValues) || forceSend || (reg.mReadErrors != 0)) {
            MsgRegisterValues val(slaveId, reg.mRegisterType, reg.mRegister, newValues);
            sendMessage(QueueItem::create(val));
            reg.update(newValues);
            reg.mReadErrors = 0;
            BOOST_LOG_SEV(log, Log::debug) << "Register " << slaveId << "." << reg.mRegister
                << " values sent, data=" << DebugTools::registersToStr(reg.getValues());
        };
    } catch (const ModbusReadException& ex) {
        handleRegisterReadError(slaveId, reg, ex.what());
    }
};

void
ModbusExecutor::handleRegisterReadError(int slaveId, RegisterPoll& regPoll, const char* errorMessage) {
    // avoid flooding logs with register read error messages - log last error every 5 minutes
    regPoll.mReadErrors++;
    if (regPoll.mReadErrors == 1 || (std::chrono::steady_clock::now() - regPoll.mFirstErrorTime > RegisterPoll::DurationBetweenLogError)) {
        BOOST_LOG_SEV(log, Log::error) << regPoll.mReadErrors << " error(s) when reading register "
            << slaveId << "." << regPoll.mRegister << ", last error: " << errorMessage;
        regPoll.mFirstErrorTime = std::chrono::steady_clock::now();
        if (regPoll.mReadErrors != 1)
            regPoll.mReadErrors = 0;
    }

    // start sending MsgRegisterReadFailed if we cannot read register DefaultReadErrorCount times
    if (regPoll.mReadErrors > RegisterPoll::DefaultReadErrorCount) {
        MsgRegisterReadFailed msg(slaveId, regPoll.mRegisterType, regPoll.mRegister, regPoll.getCount());
        sendMessage(QueueItem::create(msg));
    }
}

std::chrono::steady_clock::duration
ModbusExecutor::pollNext() {
    if (mWaitingRegister != nullptr) {
        if (
            (mWaitingRegister->getDelay().delay_type == ModbusRequestDelay::DelayType::EVERY_READ)
            ||
            (
                mWaitingRegister->getDelay().delay_type == ModbusRequestDelay::DelayType::FIRST_READ
                && mLastQueue != mQueues.end()
                && mCurrentQueue->first != mLastQueue->first
            )
        ) {
            auto delay_passed = std::chrono::steady_clock::now() - mLastPollTime;
            auto delay_left = mWaitingRegister->getDelay() - delay_passed;
            if (delay_left > std::chrono::steady_clock::duration::zero())
                return delay_left;
        }
        //mWaitingRegister is ready to be read or written
        sendCommand();
    } else {
        // find next non empty queue and start sending requests from it
        if (mCurrentQueue != mQueues.end()) {
            if (mCommandsLeft == 0 || mCurrentQueue->second.empty()) {
                auto nextQueue = mCurrentQueue;
                nextQueue++;
                while(nextQueue != mCurrentQueue) {
                    if (nextQueue == mQueues.end())
                        nextQueue = mQueues.begin();

                    if (!nextQueue->second.empty()) {
                        break;
                    }
                    nextQueue++;
                }

                if (mCurrentQueue != nextQueue) {
                    mCurrentQueue = nextQueue;
                    mWaitingRegister = mCurrentQueue->second.popNext();
                    mCommandsLeft = mBatchSize;
                } else {
                    //nothing to do
                    return std::chrono::steady_clock::duration::max();
                }
            } else {
                mWaitingRegister = mCurrentQueue->second.popNext();
            }
        }

        if (mWaitingRegister != nullptr) {
            if (mWaitingRegister->getDelay() != std::chrono::steady_clock::duration::zero()) {
                // setup and return needed delay
                // or pull register if there was enough silence
                // after previous pull
                auto last_silence_period = std::chrono::steady_clock::now() - mLastPollTime;
                if (last_silence_period < mWaitingRegister->getDelay()) {
                    auto delay_left = mWaitingRegister->getDelay() - last_silence_period;
                    return delay_left;
                }
            }
            sendCommand();
        }
    }

    if (mInitialPoll && pollDone()) {
        if (mCurrentQueue == mQueues.end()) {
            BOOST_LOG_SEV(log, Log::info) << "Nothing to do for initial poll";
        } else {
            auto end = std::chrono::steady_clock::now();
            BOOST_LOG_SEV(log, Log::info) << "Initial poll done in " << std::chrono::duration_cast<std::chrono::milliseconds>(end - mPollStart).count() << "ms";
        }
    }

    return std::chrono::steady_clock::duration::zero();
}

void
ModbusExecutor::sendCommand() {
    if (typeid(*mWaitingRegister) == typeid(RegisterPoll)) {
        pollRegister(mCurrentQueue->first, static_cast<RegisterPoll&>(*mWaitingRegister), mInitialPoll);
    } else {
        //TODO write
    }
    mLastQueue = mCurrentQueue;
    mWaitingRegister.reset();
    mCommandsLeft--;
}


bool
ModbusExecutor::allDone() const {
    if (mWaitingRegister != nullptr)
        return false;

    auto non_empty = std::find_if(mQueues.begin(), mQueues.end(),
        [](const auto& queue) -> bool { return !(queue.second.empty()); }
    );

    return non_empty == mQueues.end();
}

bool
ModbusExecutor::pollDone() const {
    auto non_empty = std::find_if(mQueues.begin(), mQueues.end(),
        [](const auto& queue) -> bool { return !(queue.second.mPollQueue.empty()); }
    );

    return non_empty == mQueues.end();
}

void
ModbusExecutor::setBatchSize(int size) {
    mBatchSize = size;
    mCommandsLeft = size;
}


}
