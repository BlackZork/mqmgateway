#include "modbus_watchdog.hpp"

namespace modmqttd {

boost::log::sources::severity_logger<Log::severity> ModbusWatchdog::log;

void
ModbusWatchdog::init(const ModbusWatchdogConfig& conf) {
    mConfig = conf;
    reset();
    BOOST_LOG_SEV(log, Log::debug) << "Watchdog initialized. Watch period set to "
        << std::chrono::duration_cast<std::chrono::seconds>(mConfig.mWatchPeriod).count() << "s";
}


void
ModbusWatchdog::inspectCommand(const RegisterCommand& command) {
    //TODO check if RTU device exists in 300ms intervals, if not then disconnect
    if (command.executedOk()) {
        reset();
    }
}


bool
ModbusWatchdog::isReconnectRequired() const {
    auto error_p = getCurrentErrorPeriod();
    BOOST_LOG_SEV(log, Log::trace) << "Watchdog: current error period is "
        << std::chrono::duration_cast<std::chrono::milliseconds>(error_p).count() << "ms";

    return error_p > mConfig.mWatchPeriod;
}


void
ModbusWatchdog::reset() {
    mLastSuccessfulCommandTime = std::chrono::steady_clock::now();
}


}
