#pragma once

#include "common.hpp"
#include "register_poll.hpp"
#include <deque>

namespace modmqttd {

class ModbusRequestsQueues {
    public:
        // set a list of registers from next poll
        void addPollList(const std::vector<std::shared_ptr<RegisterPoll>>& pollList);

        // remove next RegisterPoll from queue and return it
        // if mPollQueue is empty then move all registers from
        // mNextPollQueue and return the first one
        std::shared_ptr<RegisterPoll> popNext();

        bool empty() const { return mPollQueue.empty(); }

        // registers to poll next
        std::deque<std::shared_ptr<RegisterPoll>> mPollQueue;

        int mMaxWriteQueueSize = 1000;
};

}
