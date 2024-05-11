#pragma once

#include <deque>

#include "common.hpp"
#include "register_poll.hpp"

namespace modmqttd {

class ModbusRequestsQueues {
    public:
        // set a list of registers from next poll
        void addPollList(const std::vector<std::shared_ptr<RegisterPoll>>& pollList);

        // shared_ptr because RegisterWrite will be long-lived object
        // just like RegisterPoll. ModbusThread will maintain a list of writeRequests just like PollSpecification
        // to count and log write errors in 5min timeframes
        void addWriteCommand(const std::shared_ptr<RegisterWrite>& pReq);

        void readdCommand(const std::shared_ptr<RegisterCommand>& pCmd);

        // find the smallest positive difference between silence_period and delay need for register in queue.
        std::chrono::steady_clock::duration findForSilencePeriod(std::chrono::steady_clock::duration pPeriod, bool ignore_first_read);

        // pop the first register with pDelay
        // uses popNext() if pDelay is not found in queue
        std::shared_ptr<RegisterCommand> popFirstWithDelay(std::chrono::steady_clock::duration pPeriod, bool ignore_first_read);

        // remove next RegisterPoll from queue and return it
        // if mPollQueue is empty then move all registers from
        // mNextPollQueue and return the first one
        std::shared_ptr<RegisterCommand> popNext();

        bool empty() const { return mPollQueue.empty() && mWriteQueue.empty(); }

        // registers to poll next
        std::deque<std::shared_ptr<RegisterPoll>> mPollQueue;

        std::deque<std::shared_ptr<RegisterWrite>> mWriteQueue;
    private:
        //cache for popFirstWithDelay
        std::deque<std::shared_ptr<RegisterPoll>>::iterator mLastPollFound;

        template<typename T> std::shared_ptr<RegisterCommand> popNext(T& queue);

        // if true then popNext will get element from mPollQueue,
        // otherwise from mWriteQueue
        bool mPopFromPoll = true;
};

}
