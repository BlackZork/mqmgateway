#include "libmodmqttsrv/modbus_slave.hpp"
#include "libmodmqttsrv/yaml_converters.hpp"
#include "config.hpp"

namespace modmqttd {

ModbusSlaveConfig::ModbusSlaveConfig(const YAML::Node& data) {
    mAddress = ConfigTools::readRequiredValue<int>(data, "address");
    ConfigTools::readOptionalValue<std::chrono::milliseconds>(mDelayBeforePoll, data, "delay_before_poll");
    ConfigTools::readOptionalValue<std::chrono::milliseconds>(mDelayBeforeFirstPoll, data, "delay_before_first_poll");
}

}
