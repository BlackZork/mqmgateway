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
RegisterPoll::updateFromSlaveConfig(const ModbusSlaveConfig& slave_config) {
    if (slave_config.mDelayBeforeCommand != std::chrono::milliseconds::zero()) {
        mDelay = slave_config.mDelayBeforeCommand;
        mDelay.delay_type = ModbusCommandDelay::EVERYTIME;
    } else if (slave_config.mDelayBeforeFirstCommand != std::chrono::milliseconds::zero()) {
        mDelay = slave_config.mDelayBeforeFirstCommand;
        mDelay.delay_type = ModbusCommandDelay::ON_SLAVE_CHANGE;
    }
}

} // namespace
