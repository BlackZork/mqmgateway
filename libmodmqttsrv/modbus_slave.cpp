#include "libmodmqttsrv/modbus_slave.hpp"
#include "config.hpp"

namespace modmqttd {

ModbusSlaveConfig::ModbusSlaveConfig(const YAML::Node& data) {
    mAddress = ConfigTools::readRequiredValue<int>(data, "address");
}

}
