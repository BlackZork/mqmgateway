#include "modbus_request_queues.hpp"

namespace modmqttd {

void
ModbusRequestsQueues::addPollList(const std::vector<std::shared_ptr<RegisterPoll>>& pollList) {
    for (auto& regPollPtr: pollList) {

        std::deque<std::shared_ptr<RegisterPoll>>::const_iterator it = std::find(
            mPollQueue.begin(), mPollQueue.end(), regPollPtr
        );

        if (it == mPollQueue.end()) {
            mPollQueue.push_back(regPollPtr);
        }
    }
}

std::shared_ptr<RegisterCommand>
ModbusRequestsQueues::popNext() {
    std::shared_ptr<RegisterCommand> ret;
    if (mPopFromPoll) {
        if (mPollQueue.empty()) {
            ret = popNext(mWriteQueue);
        } else {
            mPopFromPoll = false;
            ret = popNext(mPollQueue);
        }
    } else {
        if (mWriteQueue.empty()) {
            ret = popNext(mPollQueue);
        } else {
            mPopFromPoll = true;
            ret = popNext(mWriteQueue);
        }
    }
    return ret;
}


template<typename T>
std::shared_ptr<RegisterCommand>
ModbusRequestsQueues::popNext(T& queue) {
    assert(!queue.empty());
    std::shared_ptr<RegisterCommand> ret(queue.front());
    queue.pop_front();
    return ret;
}

std::chrono::steady_clock::duration
ModbusRequestsQueues::findForSilencePeriod(std::chrono::steady_clock::duration pPeriod, bool ignore_first_read) {
    auto ret = std::chrono::steady_clock::duration::max();
    for(auto pi = mPollQueue.begin(); pi != mPollQueue.end(); pi++) {
        if (!(*pi)->hasDelay())
            continue;
        if (ignore_first_read  && (*pi)->getDelay().delay_type == ModbusCommandDelay::DelayType::ON_SLAVE_CHANGE)
            continue;

#if __cplusplus < 201703L
        auto diff = (*pi)->getDelay() - pPeriod;
        if (diff < diff.zero())
            diff = - diff;
#else
        auto diff = std::chrono::abs((*pi)->getDelay() - pPeriod);
#endif
        if (diff == std::chrono::steady_clock::duration::zero()) {
            ret = (*pi)->getDelay();
            mLastPollFound = pi;
            break;
        } else if (diff < ret) {
            ret = (*pi)->getDelay();
            mLastPollFound = pi;
        }
    }
    return ret;
}

std::shared_ptr<RegisterCommand>
ModbusRequestsQueues::popFirstWithDelay(std::chrono::steady_clock::duration pPeriod, bool ignore_first_read) {
    auto ret = std::shared_ptr<RegisterCommand>();
check_cache:
    if (mLastPollFound != mPollQueue.end()) {
        ret = *mLastPollFound;
        mPollQueue.erase(mLastPollFound);
    } else {
        findForSilencePeriod(pPeriod, ignore_first_read);
        goto check_cache;
    }
    return ret;
}

void
ModbusRequestsQueues::addWriteCommand(const std::shared_ptr<RegisterWrite>& pReq) {
    mWriteQueue.push_back(pReq);
}


void 
ModbusRequestsQueues::readdCommand(const std::shared_ptr<RegisterCommand>& pCmd) {
    if (typeid(*pCmd) == typeid(RegisterPoll)) {
        mPollQueue.push_front(std::static_pointer_cast<RegisterPoll>(pCmd));
        mPopFromPoll = true;
    } else {
        mWriteQueue.push_front(std::static_pointer_cast<RegisterWrite>(pCmd));
        mPopFromPoll = false;
    }
}

}
