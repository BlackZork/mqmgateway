#include "modbus_watchdog.hpp"

namespace modmqttd {

#if __cplusplus < 201703L
constexpr std::chrono::milliseconds ModbusWatchdog::sDeviceCheckPeriod;
#endif

void
ModbusWatchdog::init(const ModbusWatchdogConfig& conf) {
    mConfig = conf;
    reset();
    spdlog::info("Watchdog initialized. Watch period set to {}",
        std::chrono::duration_cast<std::chrono::seconds>(mConfig.mWatchPeriod)
    );
    if (!mConfig.mDevicePath.empty()) {
        spdlog::debug("Monitoring {} existence", mConfig.mDevicePath);
    }
}


void
ModbusWatchdog::inspectCommand(const RegisterCommand& command) {
    if (command.executedOk()) {
        reset();
    } else {
        // TODO remove this hack and create separate thread on ModMqtt level
        // that will monitor USB plug/unplug events using netlink or inotify
        // or https://github.com/erikzenker/inotify-cpp
        if (!mConfig.mDevicePath.empty()) {
            auto now = std::chrono::steady_clock::now();
            if (!mDeviceRemoved && (mLastCommandOk || (now - mLastDeviceCheckTime) > sDeviceCheckPeriod)) {
                mDeviceRemoved = !std::filesystem::exists(mConfig.mDevicePath.c_str());
                mLastDeviceCheckTime = now;
                if (mDeviceRemoved) {
                    spdlog::warn("Detected device {} removal", mConfig.mDevicePath);
                }
            }
        }
    }

    mLastCommandOk = command.executedOk();
}


bool
ModbusWatchdog::isReconnectRequired() const {
    if (mDeviceRemoved)
        return true;

    auto error_p = getCurrentErrorPeriod();
    spdlog::trace("Watchdog: current error period is {}",
        std::chrono::duration_cast<std::chrono::milliseconds>(error_p)
    );

    return error_p > mConfig.mWatchPeriod;
}


void
ModbusWatchdog::reset() {
    mLastSuccessfulCommandTime = std::chrono::steady_clock::now();
    mDeviceRemoved = false;
    mLastCommandOk = true;
}


}
