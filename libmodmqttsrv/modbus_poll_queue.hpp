#pragma once

#include "common.hpp"
#include "register_poll.hpp"
#include <deque>

namespace modmqttd {

class ModbusPollQueue {
    public:
        // set a list of registers from next poll
        void addPollList(const std::vector<std::shared_ptr<RegisterPoll>>& pollList);

        // remove next RegisterPoll from queue and return it
        // if mPollQueue is empty then move all registers from
        // mNextPollQueue and return the first one
        std::shared_ptr<RegisterPoll> popNext();

        bool empty() const { return mWaitingList.empty() && mPollQueue.empty(); }

        // registers waiting to be polled after mPollQueue becomes empty
        // multiple addPollList calls will update this list removing duplicates
        std::vector<std::shared_ptr<RegisterPoll>> mWaitingList;
        // registers to poll next
        std::deque<std::shared_ptr<RegisterPoll>> mPollQueue;
};

}
