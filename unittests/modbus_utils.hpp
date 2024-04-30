#pragma once

#include "libmodmqttsrv/register_poll.hpp"
#include "libmodmqttsrv/modbus_types.hpp"

class ModbusExecutorTestRegisters : public std::map<int, std::vector<std::shared_ptr<modmqttd::RegisterPoll>>>
{
    public:
        std::shared_ptr<modmqttd::RegisterPoll> add(
            int slave,
            int number,
            std::chrono::milliseconds refresh = std::chrono::milliseconds(10)
        ) {
            //TODO no check if already on list
            std::shared_ptr<modmqttd::RegisterPoll> reg(new modmqttd::RegisterPoll(number-1, modmqttd::RegisterType::HOLDING, 1, refresh));
            (*this)[slave].push_back(reg);
            return reg;
        }

        std::shared_ptr<modmqttd::RegisterPoll> addDelayed(
            int slave,
            int number,
            std::chrono::steady_clock::duration delayBeforePoll = std::chrono::milliseconds::zero(),
            modmqttd::ModbusCommandDelay::DelayType delayType = modmqttd::ModbusCommandDelay::DelayType::EVERYTIME,
            std::chrono::milliseconds refresh = std::chrono::milliseconds(10)
        ) {
            //TODO no check if already on list
            std::shared_ptr<modmqttd::RegisterPoll> reg(new modmqttd::RegisterPoll(number-1, modmqttd::RegisterType::HOLDING, 1, refresh));
            reg->mDelay = delayBeforePoll;
            reg->mDelay.delay_type = delayType;
            (*this)[slave].push_back(reg);
            return reg;
        }

};
