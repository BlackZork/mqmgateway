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
    mDelayBeforePoll = std::chrono::milliseconds::zero();
};

void
RegisterPoll::updateSlaveConfig(const ModbusSlaveConfig& slave_config) {
    mDelayBeforePoll = slave_config.mDelayBeforePoll;
    mDelayType = ReadDelayType::EVERY_READ;
    if (slave_config.mDelayBeforeFirstPoll != std::chrono::milliseconds::zero()) {
        mDelayBeforePoll = slave_config.mDelayBeforeFirstPoll;
        mDelayType = ReadDelayType::FIRST_READ;
    }
}

} // namespace
