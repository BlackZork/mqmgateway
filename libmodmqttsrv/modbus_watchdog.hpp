#pragma once

#include <filesystem>

#include "imodbuscontext.hpp"
#include "modbus_messages.hpp"
#include "register_poll.hpp"

namespace modmqttd {

class ModbusWatchdog {
    public:
        void init(const ModbusWatchdogConfig& conf);

        void inspectCommand(const RegisterCommand& command);
        void reset();
        bool isReconnectRequired() const;
        bool isDeviceRemoved() const { return mDeviceRemoved; }
        const std::string& getDevicePath() const { return mConfig.mDevicePath; }
        std::chrono::steady_clock::time_point getLastSuccessfulCommandTime() const;
        std::chrono::steady_clock::duration getCurrentErrorPeriod() const {
            return std::chrono::steady_clock::now() - mLastSuccessfulCommandTime;
        }

    private:
        static constexpr std::chrono::milliseconds sDeviceCheckPeriod = std::chrono::milliseconds(300);

        ModbusWatchdogConfig mConfig;
        std::chrono::steady_clock::time_point mLastSuccessfulCommandTime;

        std::chrono::steady_clock::time_point mLastDeviceCheckTime;
        bool mLastCommandOk = true;
        bool mDeviceRemoved = false;
};


}
