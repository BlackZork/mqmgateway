#pragma once

#include <chrono>

namespace modmqttd {

enum RegisterType {
    COIL = 1,
    BIT = 2,
    HOLDING = 3,
    INPUT = 4
};


class ModbusCommandDelay : public std::chrono::steady_clock::duration {
    public:
        typedef enum {
            // wait before every request
            EVERYTIME = 0,
            // wait only if new request is for different slave than
            // previous one
            ON_SLAVE_CHANGE = 1
        } DelayType;

        ModbusCommandDelay() {}
        ModbusCommandDelay(const std::chrono::steady_clock::duration& dur, DelayType dt = DelayType::EVERYTIME)
            : std::chrono::steady_clock::duration(dur),
            delay_type(dt)
        {}
        ModbusCommandDelay& operator=(const std::chrono::steady_clock::duration& dur) {
            std::chrono::steady_clock::duration::operator= (dur);
            return *this;
        }

        DelayType delay_type = DelayType::EVERYTIME;
};

}
