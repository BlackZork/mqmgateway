#include "modbus_watchdog.hpp"

namespace modmqttd {

void
ModbusWatchdog::init(const ModbusWatchdogConfig& conf, const std::shared_ptr<IModbusContext>& modbus) {
    mModbus = modbus;
    mConfig = conf;
}

void
ModbusWatchdog::inspectCommand(const IRegisterCommand& command) {
    //TODO check if RTU device exists in 300ms intervals, if not then disconnect
    //check for watchdog_period if any command succeed, if not then disconnect
    throw std::logic_error("not implemented");
}

}
