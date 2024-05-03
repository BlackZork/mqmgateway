#include "register_poll.hpp"

namespace modmqttd {

constexpr std::chrono::steady_clock::duration RegisterPoll::DurationBetweenLogError;

RegisterPoll::RegisterPoll(int regNum, RegisterType regType, int regCount, std::chrono::milliseconds refreshMsec)
    : ModbusAddressRange(regNum, regType, regCount),
      mLastRead(std::chrono::steady_clock::now() - std::chrono::hours(24)),
      mLastValues(regCount)
{
    mRefresh = refreshMsec;
    mReadErrors = 0;
    mFirstErrorTime = std::chrono::steady_clock::now();
    mDelay = std::chrono::milliseconds::zero();
};

} // namespace
