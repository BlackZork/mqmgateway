#include <iomanip>

#include "modbus_executor.hpp"
#include "modbus_messages.hpp"
#include "modbus_thread.hpp"
#include "modbus_types.hpp"
#include "queue_item.hpp"

namespace modmqttd {

boost::log::sources::severity_logger<Log::severity> ModbusExecutor::log;

#if __cplusplus < 201703L
    constexpr std::chrono::milliseconds ModbusExecutor::WRITE_BATCH_SIZE;
#endif

ModbusExecutor::ModbusExecutor(
    moodycamel::BlockingReaderWriterQueue<QueueItem>& fromModbusQueue,
    moodycamel::BlockingReaderWriterQueue<QueueItem>& toModbusQueue
)
    : mFromModbusQueue(fromModbusQueue), mToModbusQueue(toModbusQueue)
{
    //some random past value, not using steady_clock:min() due to overflow
    mLastPollTime = std::chrono::steady_clock::now() - std::chrono::hours(100000);
    mLastQueue = mSlaveQueues.end();
    mCurrentSlaveQueue = mSlaveQueues.end();
    mInitialPoll = false;
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

    bool setupQueues = pollDone();

    if (mInitialPoll) {
        if (!pollDone()) {
            BOOST_LOG_SEV(log, Log::error) << "Cannot add next registers before initial poll is finished. Fix control loop.";
            return;
        }
    }

    if (initialPoll) {
        mInitialPoll = initialPoll;
        mPollStart = std::chrono::steady_clock::now();
    }

    for (auto& pit: pRegisters) {
        auto& queue = mSlaveQueues[pit.first];
        queue.addPollList(pit.second);
    }

    // we are already polling data or have nothing to do
    // so do not try to find what to read or write next
    if (!setupQueues || allDone()) {
        return;
    }

    mCurrentSlaveQueue = mSlaveQueues.begin();

    // scan register list for registers that have delay_before_poll set
    // and find the best one that fits in the last_silence_period
    auto last_silence_period = std::chrono::steady_clock::now() - mLastPollTime;

    BOOST_LOG_SEV(log, Log::trace) << "Starting election for silence period " << std::chrono::duration_cast<std::chrono::milliseconds>(last_silence_period).count() << "ms";

    mWaitingRegister = nullptr;

    auto currentDiff = std::chrono::steady_clock::duration::max();

    for(auto sit = mSlaveQueues.begin(); sit != mSlaveQueues.end(); sit++) {
        bool ignore_first_read = (sit == mCurrentSlaveQueue);

        if (currentDiff != std::chrono::steady_clock::duration::zero()) {
            ModbusCommandDelay reg_delay = sit->second.findForSilencePeriod(last_silence_period, ignore_first_read);

            if (reg_delay < currentDiff) {
                std::shared_ptr<IRegisterCommand> reg(sit->second.popFirstWithDelay(last_silence_period, ignore_first_read));
                mWaitingRegister = reg;
                mCurrentSlaveQueue = sit;
                currentDiff = reg_delay;
                BOOST_LOG_SEV(log, Log::trace) << "Electing next register to poll as " << mCurrentSlaveQueue->first << "." << mWaitingRegister->getRegister()
                    << ", delay=" << std::chrono::duration_cast<std::chrono::milliseconds>(reg_delay).count() << "ms"
                    << ", diff=" << std::chrono::duration_cast<std::chrono::milliseconds>(currentDiff).count() << "ms";
                if (reg_delay.count() == 0)
                    break;
            }
        }
    }

    resetCommandsCounter();


    //if there are no registers with delay set start from the first queue
    if (currentDiff == std::chrono::steady_clock::duration::max()) {
        mCurrentSlaveQueue = mSlaveQueues.begin();
        mWaitingRegister = mCurrentSlaveQueue->second.popNext();
    }


    BOOST_LOG_SEV(log, Log::trace) << "Next register to poll set to " << mCurrentSlaveQueue->first << "." << mWaitingRegister->getRegister() << ", commands_left=" << mCommandsLeft;
}


void
ModbusExecutor::addWriteCommand(int slaveId, const std::shared_ptr<RegisterWrite>& pCommand) {
    auto& queue = mSlaveQueues[slaveId];
    queue.addWriteCommand(pCommand);
    if (mCurrentSlaveQueue == mSlaveQueues.end())
        mCurrentSlaveQueue = mSlaveQueues.begin();
}


void
ModbusExecutor::pollRegisters(int slaveId, RegisterPoll& reg, bool forceSend) {
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

void
ModbusExecutor::writeRegisters(int slaveId, const RegisterWrite& cmd) {
    try {
        std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        mModbus->writeModbusRegisters(slaveId, cmd);
        mLastPollTime = std::chrono::steady_clock::now();

        std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
        BOOST_LOG_SEV(log, Log::debug) << "Register " << slaveId << "." << cmd.mRegister << " (0x" << std::hex << slaveId << ".0x" << std::hex << cmd.mRegister << ")"
                        << " written in "  << std::dec << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms";

        if (cmd.mReturnMessage != nullptr) {
            cmd.mReturnMessage->mRegisters = ModbusRegisters(cmd.getValues());
            sendMessage(QueueItem::create(*cmd.mReturnMessage));
        }
    } catch (const ModbusWriteException& ex) {
        BOOST_LOG_SEV(log, Log::error) << "error writing register "
            << slaveId << "." << cmd.mRegister << ": " << ex.what();
        MsgRegisterWriteFailed msg(slaveId, cmd.mRegisterType, cmd.mRegister, cmd.getCount());
        sendMessage(QueueItem::create(msg));
    }
}


std::chrono::steady_clock::duration
ModbusExecutor::pollNext() {
    if (mWaitingRegister != nullptr) {
        if (
            (mWaitingRegister->getDelay().delay_type == ModbusCommandDelay::DelayType::EVERYTIME)
            ||
            (
                mWaitingRegister->getDelay().delay_type == ModbusCommandDelay::DelayType::ON_SLAVE_CHANGE
                && mLastQueue != mSlaveQueues.end()
                && mCurrentSlaveQueue->first != mLastQueue->first
            )
        ) {
            auto delay_passed = std::chrono::steady_clock::now() - mLastPollTime;
            auto delay_left = mWaitingRegister->getDelay() - delay_passed;
            if (delay_left > std::chrono::steady_clock::duration::zero()) {
                BOOST_LOG_SEV(log, Log::trace) << "Command for " << mCurrentSlaveQueue->first << "." << mWaitingRegister->getRegister()
                    << " need to wait " << std::chrono::duration_cast<std::chrono::milliseconds>(delay_left).count() << "ms";
                return delay_left;
            }
        }
        //mWaitingRegister is ready to be read or written
        sendCommand();
    } else {
        // find next non empty queue and start sending requests from it
        if (mCurrentSlaveQueue != mSlaveQueues.end()) {
            if (mCommandsLeft == 0 || mCurrentSlaveQueue->second.empty()) {
                auto nextQueue = mCurrentSlaveQueue;
                nextQueue++;
                while(nextQueue != mCurrentSlaveQueue) {
                    if (nextQueue == mSlaveQueues.end())
                        nextQueue = mSlaveQueues.begin();

                    if (!nextQueue->second.empty()) {
                        break;
                    }
                    nextQueue++;
                }

                // if mCurrentSlaveQueue was left due to mCommandsLeft==0
                // nextQueue could point at it again after doing full circle
                if (mCurrentSlaveQueue != nextQueue || !mCurrentSlaveQueue->second.empty()) {
                    mCurrentSlaveQueue = nextQueue;
                    mWaitingRegister = mCurrentSlaveQueue->second.popNext();
                    resetCommandsCounter();
                } else {
                    //nothing to do
                    return std::chrono::steady_clock::duration::max();
                }
            } else {
                mWaitingRegister = mCurrentSlaveQueue->second.popNext();
            }
        }

        if (mWaitingRegister != nullptr) {
            if (mWaitingRegister->getDelay() != std::chrono::steady_clock::duration::zero()) {
                // setup and return needed delay
                // or pull register if there was enough silence
                // after previous command
                auto last_silence_period = std::chrono::steady_clock::now() - mLastPollTime;
                if (last_silence_period < mWaitingRegister->getDelay()) {
                    auto delay_left = mWaitingRegister->getDelay() - last_silence_period;

                    BOOST_LOG_SEV(log, Log::trace) << "Next register set to " << mCurrentSlaveQueue->first << "." << mWaitingRegister->getRegister()
                        << ", delay=" << std::chrono::duration_cast<std::chrono::milliseconds>(mWaitingRegister->getDelay()).count() << "ms"
                        << ", left=" << std::chrono::duration_cast<std::chrono::milliseconds>(delay_left).count() << "ms";
                    return delay_left;
                }
            }
            sendCommand();
        }
    }

    if (mInitialPoll && pollDone()) {
        if (mCurrentSlaveQueue == mSlaveQueues.end()) {
            BOOST_LOG_SEV(log, Log::info) << "Nothing to do for initial poll";
        } else {
            auto end = std::chrono::steady_clock::now();
            BOOST_LOG_SEV(log, Log::info) << "Initial poll done in " << std::chrono::duration_cast<std::chrono::milliseconds>(end - mPollStart).count() << "ms";
            mInitialPoll = false;
        }
    }

    return std::chrono::steady_clock::duration::zero();
}

void
ModbusExecutor::sendCommand() {
    if (typeid(*mWaitingRegister) == typeid(RegisterPoll)) {
        pollRegisters(mCurrentSlaveQueue->first, static_cast<RegisterPoll&>(*mWaitingRegister), mInitialPoll);
    } else {
        writeRegisters(mCurrentSlaveQueue->first, static_cast<RegisterWrite&>(*mWaitingRegister));
    }
    mLastQueue = mCurrentSlaveQueue;
    mWaitingRegister.reset();
    mCommandsLeft--;
}


bool
ModbusExecutor::allDone() const {
    if (mWaitingRegister != nullptr)
        return false;

    auto non_empty = std::find_if(mSlaveQueues.begin(), mSlaveQueues.end(),
        [](const auto& queue) -> bool { return !(queue.second.empty()); }
    );

    return non_empty == mSlaveQueues.end();
}

bool
ModbusExecutor::pollDone() const {
    auto non_empty = std::find_if(mSlaveQueues.begin(), mSlaveQueues.end(),
        [](const auto& queue) -> bool { return !(queue.second.mPollQueue.empty()); }
    );

    return non_empty == mSlaveQueues.end();
}

void
ModbusExecutor::resetCommandsCounter() {
    if (mCurrentSlaveQueue->second.mPollQueue.empty())
        mCommandsLeft = WRITE_BATCH_SIZE;
    else
        mCommandsLeft = mCurrentSlaveQueue->second.mPollQueue.size() * 2;
}


}
