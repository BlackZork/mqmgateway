#pragma once

#include "libmodmqttsrv/register_poll.hpp"
#include "libmodmqttsrv/modbus_types.hpp"

class ModbusExecutorTestRegisters : public std::map<int, std::vector<std::shared_ptr<modmqttd::RegisterPoll>>>
{
    public:
        std::shared_ptr<modmqttd::RegisterPoll> addPoll(
            int slave,
            int number,
            std::chrono::milliseconds refresh = std::chrono::milliseconds(10)
        ) {
            //TODO no check if already on list
            std::shared_ptr<modmqttd::RegisterPoll> reg(new modmqttd::RegisterPoll(number-1, modmqttd::RegisterType::HOLDING, 1, refresh));
            (*this)[slave].push_back(reg);
            return reg;
        }

        std::shared_ptr<modmqttd::RegisterPoll> addPollDelayed(
            int slave,
            int number,
            std::chrono::steady_clock::duration delay = std::chrono::milliseconds::zero(),
            modmqttd::ModbusCommandDelay::DelayType delayType = modmqttd::ModbusCommandDelay::DelayType::EVERYTIME,
            std::chrono::milliseconds refresh = std::chrono::milliseconds(10)
        ) {
            //TODO no check if already on list
            std::shared_ptr<modmqttd::RegisterPoll> reg(new modmqttd::RegisterPoll(number-1, modmqttd::RegisterType::HOLDING, 1, refresh));
            modmqttd::ModbusCommandDelay md(delay);
            md.delay_type = delayType;
            reg->setDelay(md);
            (*this)[slave].push_back(reg);
            return reg;
        }


        static std::shared_ptr<modmqttd::RegisterWrite> createWrite(
            int number,
            uint16_t value
        ) {
            std::shared_ptr<modmqttd::RegisterWrite> reg(new modmqttd::RegisterWrite(number-1, modmqttd::RegisterType::HOLDING, value));
            return reg;
        }

        static std::shared_ptr<modmqttd::RegisterWrite> createWriteDelayed (
            int number,
            uint16_t value,
            std::chrono::steady_clock::duration delay = std::chrono::milliseconds::zero(),
            modmqttd::ModbusCommandDelay::DelayType delayType = modmqttd::ModbusCommandDelay::DelayType::EVERYTIME
        ) {
            std::shared_ptr<modmqttd::RegisterWrite> reg(new modmqttd::RegisterWrite(number-1, modmqttd::RegisterType::HOLDING, value));
            modmqttd::ModbusCommandDelay md(delay);
            md.delay_type = delayType;
            reg->setDelay(md);
            return reg;
        }
};
