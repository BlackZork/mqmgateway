#pragma once

#include <chrono>
#include <vector>
#include "modbus_types.hpp"
#include "modbus_slave.hpp"

namespace modmqttd {

class RegisterPoll {
    public:
        typedef enum {
            // wait mDelayBeforePoll before every read from this slave
            EVERY_READ = 0,
            // respect mDelayBeforePoll only before the first read from this slave
            // subsequent reads will be executed without any delay.
            FIRST_READ = 1
        } ReadDelayType;

        static constexpr std::chrono::steady_clock::duration DurationBetweenLogError = std::chrono::minutes(5);
        // if we cannot read register in this time MsgRegisterReadFailed is sent
        static constexpr int DefaultReadErrorCount = 3;

        RegisterPoll(int regNum, RegisterType regType, int regCount, std::chrono::milliseconds refreshMsec);
        int getCount() const { return mLastValues.size(); }
        const std::vector<uint16_t>& getValues() const { return mLastValues; }
        void update(const std::vector<uint16_t> newValues) { mLastValues = newValues; }
        void updateSlaveConfig(const ModbusSlaveConfig& slave_config);

        int mRegister;
        RegisterType mRegisterType;
        std::chrono::steady_clock::duration mRefresh;

        std::chrono::steady_clock::duration mDelayBeforePoll;
        ReadDelayType mDelayType = ReadDelayType::EVERY_READ;

        std::chrono::steady_clock::time_point mLastRead;

        int mReadErrors;
        std::chrono::steady_clock::time_point mFirstErrorTime;
    private:
        std::vector<uint16_t> mLastValues;
};

} //namespace
