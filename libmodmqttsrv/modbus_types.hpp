#pragma once

#include <chrono>

namespace modmqttd {

enum RegisterType {
    COIL = 1,
    BIT = 2,
    HOLDING = 3,
    INPUT = 4
};


class ModbusRequestDelay : public std::chrono::steady_clock::duration {
    public:
        typedef enum {
            // wait before every request
            EVERY_READ = 0,
            // wait only if new request is for different slave than
            // previous one
            FIRST_READ = 1
        } DelayType;

        ModbusRequestDelay() {}
        ModbusRequestDelay(const std::chrono::steady_clock::duration& dur, DelayType dt = DelayType::EVERY_READ)
            : std::chrono::steady_clock::duration(dur),
            delay_type(dt)
        {}
        ModbusRequestDelay& operator=(const std::chrono::steady_clock::duration& dur) {
            std::chrono::steady_clock::duration::operator= (dur);
            return *this;
        }

        DelayType delay_type = DelayType::EVERY_READ;
};

}
