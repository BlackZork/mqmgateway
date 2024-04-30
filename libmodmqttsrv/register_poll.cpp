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
    mDelay = std::chrono::milliseconds::zero();
};

void
RegisterPoll::updateSlaveConfig(const ModbusSlaveConfig& slave_config) {
    mDelay = slave_config.mDelayBeforePoll;
    mDelay.delay_type = ModbusRequestDelay::DelayType::EVERY_READ;
    if (slave_config.mDelayBeforeFirstPoll != std::chrono::milliseconds::zero()) {
        mDelay = slave_config.mDelayBeforeFirstPoll;
        mDelay.delay_type = ModbusRequestDelay::DelayType::FIRST_READ;
    }
}

} // namespace
