
#include "modbus_types.hpp"

namespace modmqttd {

bool
ModbusAddressRange::overlaps(const ModbusAddressRange& poll) const {
    if (mRegisterType != poll.mRegisterType)
        return false;

    return (
        (firstRegister() <= poll.lastRegister()) && (poll.firstRegister() <= lastRegister())
    );
}

void
ModbusAddressRange::merge(const ModbusAddressRange& other) {
    int first = firstRegister() <= other.firstRegister() ? firstRegister() : other.firstRegister();
    int last = lastRegister() >= other.lastRegister() ? lastRegister() : other.lastRegister();

    spdlog::debug("Extending register {} ({}) to {} ({})",
        mRegister, mCount,
        first, last-first+1
    );

    mRegister = first;
    mCount = last - mRegister + 1;
}

bool
ModbusAddressRange::isConsecutiveOf(const ModbusAddressRange& other) const {
    return (lastRegister() + 1 == other.firstRegister() || other.lastRegister()+1 == firstRegister());
}

bool
ModbusAddressRange::isSameAs(const ModbusAddressRange& other) const {
    if (mRegisterType != other.mRegisterType)
        return false;

    return mRegister == other.mRegister && mCount == other.mCount;
}

}
