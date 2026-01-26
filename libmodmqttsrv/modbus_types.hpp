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

class ModbusAddressRange {
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


