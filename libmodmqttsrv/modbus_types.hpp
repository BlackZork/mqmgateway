#pragma once

#include <chrono>

#include "logging.hpp"

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

        ModbusCommandDelay() : std::chrono::steady_clock::duration(0) {}
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

class ModbusAddressRange {
    protected:
        static boost::log::sources::severity_logger<Log::severity> log;
    public:
        ModbusAddressRange(int pRegister, RegisterType pRegisterType, int pCount)
            : mRegister(pRegister), mRegisterType(pRegisterType), mCount(pCount)
        {}

        void merge(const ModbusAddressRange& other);
        bool overlaps(const ModbusAddressRange& poll) const;
        bool isConsecutiveOf(const ModbusAddressRange& other) const;
        bool isSameAs(const ModbusAddressRange& other) const;
        int firstRegister() const { return mRegister; }
        int lastRegister() const { return (mRegister + mCount) - 1; }

        int mRegister;
        int mCount;
        RegisterType mRegisterType;
};


class ModbusSlaveAddressRange : public ModbusAddressRange {
    public:
        ModbusSlaveAddressRange(int pSlaveId, int pRegisterNumber, RegisterType pType, int pCount)
            : ModbusAddressRange(pRegisterNumber, pType, pCount),
              mSlaveId(pSlaveId)
        {}

        int mSlaveId;
};

}
