#include "modbus_poller.hpp"
#include "modbus_messages.hpp"
#include "modbus_thread.hpp"
#include "queue_item.hpp"

namespace modmqttd {

ModbusPoller::ModbusPoller(moodycamel::BlockingReaderWriterQueue<QueueItem>& fromModbusQueue)
    : mFromModbusQueue(fromModbusQueue)
{}

void
ModbusPoller::sendMessage(const QueueItem& item) {
    ModbusThread::sendMessageFromModbus(mFromModbusQueue, item);
}

void
ModbusPoller::setupInitialPoll(const std::map<int, std::vector<std::shared_ptr<RegisterPoll>>>& pRegisters) {
    mLastPollTime = std::chrono::time_point<std::chrono::steady_clock>::min();
    setPollList(pRegisters, true);
    BOOST_LOG_SEV(log, Log::debug) << "starting initial poll";
    // assume max silence before initial poll
}


void
ModbusPoller::setPollList(const std::map<int, std::vector<std::shared_ptr<RegisterPoll>>>& pRegisters, bool initialPoll) {
    mRegisters = pRegisters;
    mCurrentSlave = 0;
    mInitialPoll = initialPoll;

    if (pRegisters.empty()) {
        // could happen if modbus is in write only mode
        mWaitingRegister.reset();
        mCurrentSlave = 0;
        return;
    }


    // scan register list for registers that have delay_before_poll set
    // and find the best one that fits in the last_silence_period
    auto last_silence_period = std::chrono::steady_clock::duration::max();
    if (mLastPollTime != std::chrono::time_point<std::chrono::steady_clock>::min())
        last_silence_period = std::chrono::steady_clock::now() - mLastPollTime;

    std::vector<std::shared_ptr<RegisterPoll>>::iterator selected;
    mWaitingRegister = nullptr;
    for(auto sit = mRegisters.begin(); sit != mRegisters.end(); sit++) {
        for (auto rit = sit->second.begin(); rit != sit->second.end(); rit++) {
            if ((*rit)->mDelayBeforePoll == std::chrono::steady_clock::duration::zero())
                continue;
            if ((*rit)->mDelayBeforePoll > last_silence_period)
                continue;

            if (mWaitingRegister == nullptr || mWaitingRegister->mDelayBeforePoll < (*rit)->mDelayBeforePoll) {
                selected = rit;
                mWaitingRegister = *rit;
                mCurrentSlave = sit->first;
            }
        }
    }

    // remove selected register outside of iteration loop
    if (mWaitingRegister != nullptr) {
        mRegisters[mCurrentSlave].erase(selected);
    }

    // no registers with delay before poll
    // or we will be doing initial poll
    if (mCurrentSlave == 0) {
        mCurrentSlave = mRegisters.begin()->first;
    }
    mPollStart = std::chrono::steady_clock::now();
}


void
ModbusPoller::pollRegister(int slaveId, const std::shared_ptr<RegisterPoll>& reg_ptr, bool forceSend) {
    try {
        std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        RegisterPoll& reg(*reg_ptr);
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
        handleRegisterReadError(slaveId, *reg_ptr, ex.what());
    }
};

void
ModbusPoller::handleRegisterReadError(int slaveId, RegisterPoll& regPoll, const char* errorMessage) {
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
ModbusPoller::pollNext() {
    if (mWaitingRegister != nullptr) {
        if (mLastPollTime != std::chrono::time_point<std::chrono::steady_clock>::min()) {
            auto delay_passed = std::chrono::steady_clock::now() - mLastPollTime;
            auto delay_left = mWaitingRegister->mDelayBeforePoll - delay_passed;
            if (delay_left > std::chrono::steady_clock::duration::zero())
                return delay_left;
        }
        //mWaitingRegister is ready to be polled
        pollRegister(mCurrentSlave, mWaitingRegister, mInitialPoll);
        mWaitingRegister.reset();
    } else {

        std::shared_ptr<RegisterPoll> toPoll;

        if (!mRegisters.empty()) {
            if (mCurrentSlave != 0) {
                auto& regLst = mRegisters[mCurrentSlave];
                if (regLst.empty()) {
                    mRegisters.erase(mCurrentSlave);
                    mCurrentSlave = 0;
                } else {
                    toPoll = regLst.front();
                    regLst.erase(regLst.begin());
                }
            }

            if (toPoll == nullptr) {
                mCurrentSlave = 0;
                //go to first slave with non empty list
                auto mbegin = mRegisters.begin();
                auto mcurrent = mbegin;
                while(mcurrent != mRegisters.end()) {
                    if(!mcurrent->second.empty())
                        break;
                    mcurrent++;
                }

                //erase all traversed empty lists
                if (mbegin != mcurrent) {
                    mRegisters.erase(mbegin, mcurrent);
                }

                if (mcurrent != mRegisters.end()) {
                    //we have something to poll
                    mCurrentSlave = mcurrent->first;
                    toPoll = mcurrent->second.front();
                    mcurrent->second.erase(mcurrent->second.begin());
                }
            }

            if (toPoll != nullptr) {
                if (toPoll->mDelayBeforePoll != std::chrono::steady_clock::duration::zero()) {
                    // setup and return needed delay
                    // or pull register if there was enough silence
                    // after previous pull
                    auto last_silence_period = std::chrono::steady_clock::now() - mLastPollTime;
                    if (last_silence_period < toPoll->mDelayBeforePoll) {
                        auto delay_left = toPoll->mDelayBeforePoll - last_silence_period;
                        mWaitingRegister = toPoll;
                        return delay_left;
                    } else {
                        pollRegister(mCurrentSlave, toPoll, mInitialPoll);
                    }
                } else {
                    pollRegister(mCurrentSlave, toPoll, mInitialPoll);
                }
            }
        }
    }

    if (mCurrentSlave && mRegisters[mCurrentSlave].empty()) {
        mRegisters.erase(mCurrentSlave);
        mCurrentSlave = 0;
    };

    if (mInitialPoll && mWaitingRegister == nullptr && mRegisters.empty()) {
        auto end = std::chrono::steady_clock::now();
        BOOST_LOG_SEV(log, Log::info) << "Initial poll done in " << std::chrono::duration_cast<std::chrono::milliseconds>(end - mPollStart).count() << "ms";
    }

    return std::chrono::steady_clock::duration::zero();
}

}
