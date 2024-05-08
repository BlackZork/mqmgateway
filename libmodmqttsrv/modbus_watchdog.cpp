#include "modbus_watchdog.hpp"
#include "boost/filesystem.hpp"

namespace modmqttd {

boost::log::sources::severity_logger<Log::severity> ModbusWatchdog::log;

#if __cplusplus < 201703L
constexpr std::chrono::milliseconds ModbusWatchdog::sDeviceCheckPeriod;
#endif

void
ModbusWatchdog::init(const ModbusWatchdogConfig& conf) {
    mConfig = conf;
    reset();
    BOOST_LOG_SEV(log, Log::debug) << "Watchdog initialized. Watch period set to "
        << std::chrono::duration_cast<std::chrono::seconds>(mConfig.mWatchPeriod).count() << "s";
    if (!mConfig.mDevicePath.empty()) {
        BOOST_LOG_SEV(log, Log::debug) << "Monitoring " << mConfig.mDevicePath << " existence";
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
                mDeviceRemoved = !boost::filesystem::exists(mConfig.mDevicePath.c_str());
                mLastDeviceCheckTime = now;
                BOOST_LOG_SEV(log, Log::debug) << "Detected device" << mConfig.mDevicePath << " removal";
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
    BOOST_LOG_SEV(log, Log::trace) << "Watchdog: current error period is "
        << std::chrono::duration_cast<std::chrono::milliseconds>(error_p).count() << "ms";

    return error_p > mConfig.mWatchPeriod;
}


void
ModbusWatchdog::reset() {
    mLastSuccessfulCommandTime = std::chrono::steady_clock::now();
    mDeviceRemoved = false;
    mLastCommandOk = true;
}


}
