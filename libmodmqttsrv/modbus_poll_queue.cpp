#include "modbus_poll_queue.hpp"

namespace modmqttd {

void
ModbusPollQueue::addPollList(const std::vector<std::shared_ptr<RegisterPoll>>& pollList) {
    for (auto& regPollPtr: pollList) {
        auto wit = mWaitingList.begin();
        while (wit != mWaitingList.end()) {
            if (*wit == regPollPtr)
                break;
            wit++;
        }

        if (wit == mWaitingList.end()) {
            //TODO log performance problem - we are adding
            //RegisterPoll that should already be polled
            //log should be done in periods?
            auto qit = mPollQueue.begin();
            while (qit != mPollQueue.end()) {
                if (*qit == regPollPtr)
                    return;
                qit++;
            }

            mWaitingList.push_back(regPollPtr);
        }
    }
}

std::shared_ptr<RegisterPoll>
ModbusPollQueue::popNext() {

    if (mPollQueue.empty()) {
        std::move(mWaitingList.begin(), mWaitingList.end(), std::back_inserter(mPollQueue));
        mWaitingList.clear();
    }

    std::shared_ptr<RegisterPoll> ret(mPollQueue.front());
    mPollQueue.pop_front();
    return ret;
}


}
