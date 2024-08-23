#pragma once

#include "libmodmqttsrv/register_poll.hpp"
#include "libmodmqttsrv/modbus_types.hpp"
#include "libmodmqttsrv/common.hpp"

class ModbusExecutorTestRegisters : public std::map<int, std::vector<std::shared_ptr<modmqttd::RegisterPoll>>>
{
    public:
        std::shared_ptr<modmqttd::RegisterPoll> addPoll(
            int slave,
            int number,
            std::chrono::milliseconds refresh = std::chrono::milliseconds(10)
        ) {
            //TODO no check if already on list
            std::shared_ptr<modmqttd::RegisterPoll> reg(new modmqttd::RegisterPoll(slave, number-1, modmqttd::RegisterType::HOLDING, 1, refresh, modmqttd::PublishMode::ON_CHANGE));
            (*this)[slave].push_back(reg);
            return reg;
        }

        std::shared_ptr<modmqttd::RegisterPoll> addPollDelayed(
            int slave,
            int number,
            std::chrono::steady_clock::duration delay = std::chrono::milliseconds::zero(),
            std::chrono::steady_clock::duration first_delay = std::chrono::milliseconds::zero(),
            std::chrono::milliseconds refresh = std::chrono::milliseconds(10)
        ) {
            //TODO no check if already on list
            std::shared_ptr<modmqttd::RegisterPoll> reg(new modmqttd::RegisterPoll(slave, number-1, modmqttd::RegisterType::HOLDING, 1, refresh, modmqttd::PublishMode::ON_CHANGE));
            reg->setDelayBeforeFirstCommand(first_delay);
            reg->setDelayBeforeCommand(delay);
            (*this)[slave].push_back(reg);
            return reg;
        }


        static std::shared_ptr<modmqttd::RegisterWrite> createWrite(
            int slave,
            int number,
            uint16_t value
        ) {
            std::shared_ptr<modmqttd::RegisterWrite> reg(new modmqttd::RegisterWrite(slave, number-1, modmqttd::RegisterType::HOLDING, value));
            return reg;
        }

        static std::shared_ptr<modmqttd::RegisterWrite> createWriteDelayed (
            int slave,
            int number,
            uint16_t value,
            std::chrono::steady_clock::duration delay = std::chrono::milliseconds::zero(),
            std::chrono::steady_clock::duration first_delay = std::chrono::milliseconds::zero()
        ) {
            std::shared_ptr<modmqttd::RegisterWrite> reg(new modmqttd::RegisterWrite(slave, number-1, modmqttd::RegisterType::HOLDING, value));
            reg->setDelayBeforeFirstCommand(first_delay);
            reg->setDelayBeforeCommand(delay);
            return reg;
        }
};
