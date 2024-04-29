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

std::shared_ptr<RegisterPoll>
ModbusRequestsQueues::popNext() {
    std::shared_ptr<RegisterPoll> ret(mPollQueue.front());
    mPollQueue.pop_front();
    return ret;
}


}
