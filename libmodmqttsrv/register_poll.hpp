#pragma once

#include <chrono>
#include "modbus_types.hpp"

namespace modmqttd {

class RegisterPoll {
    public:
        static constexpr std::chrono::steady_clock::duration DurationBetweenLogError = std::chrono::minutes(5);
        // if we cannot read register in this time MsgRegisterReadFailed is sent
        static constexpr int DefaultReadErrorCount = 3;
        RegisterPoll(int registerAddress, RegisterType regType, int refreshMsec);
        int mRegisterAddress;
        RegisterType mRegisterType;
        std::chrono::steady_clock::duration mRefresh;
        uint16_t mLastValue;
        std::chrono::steady_clock::time_point mLastRead;

        int mReadErrors;
        std::chrono::steady_clock::time_point mFirstErrorTime;
};

} //namespace
