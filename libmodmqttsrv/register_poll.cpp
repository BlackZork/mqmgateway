#include "register_poll.hpp"

namespace modmqttd {

constexpr std::chrono::steady_clock::duration RegisterPoll::DurationBetweenLogError;

RegisterPoll::RegisterPoll(int regNum, RegisterType regType, int regCount, std::chrono::milliseconds refreshMsec)
    : mLastRead(std::chrono::steady_clock::now() - std::chrono::hours(24)),
    mRegister(regNum),
    mRegisterType(regType),
    mLastValues(regCount)
{
    mRefresh = refreshMsec;
    mReadErrors = 0;
    mFirstErrorTime = std::chrono::steady_clock::now();
};

} // namespace
