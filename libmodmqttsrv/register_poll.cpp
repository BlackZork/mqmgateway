#include "register_poll.hpp"

namespace modmqttd {

constexpr std::chrono::steady_clock::duration RegisterPoll::DurationBetweenLogError;

void
RegisterCommand::setMaxRetryCounts(short pMaxRead, short pMaxWrite, bool pForce) {
    if (pMaxRead != 0 || pForce)
        mMaxReadRetryCount = pMaxRead;
    if (pMaxWrite || pForce)
        mMaxWriteRetryCount = pMaxWrite;
}

RegisterPoll::RegisterPoll(int pSlaveId, int pRegNum, RegisterType pRegType, int pRegCount, std::chrono::milliseconds pRefreshMsec, PublishMode pPublishMode, uint16_t errorValue)
    : RegisterCommand(pSlaveId, pRegNum, pRegType, pRegCount),
      mPublishMode(pPublishMode),
      mLastRead(std::chrono::steady_clock::now() - std::chrono::hours(24)),
      mLastValues(pRegCount),
      errorValue(errorValue)
{
    mRefresh = pRefreshMsec;
    mReadErrors = 0;
    mFirstErrorTime = std::chrono::steady_clock::now();
};

} // namespace
