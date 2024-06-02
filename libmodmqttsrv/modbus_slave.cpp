#include "modbus_slave.hpp"
#include "yaml_converters.hpp"
#include "config.hpp"

namespace modmqttd {

boost::log::sources::severity_logger<Log::severity> ModbusSlaveConfig::log;

ModbusSlaveConfig::ModbusSlaveConfig(int pAddress, const YAML::Node& data)
    : mAddress(pAddress)
{
    if (ConfigTools::readOptionalValue<std::chrono::milliseconds>(mDelayBeforeCommand, data, "delay_before_poll")) {
        BOOST_LOG_SEV(log, Log::warn) << "'delay_before_poll' is deprecated and will be removed in future releases. Rename it to 'delay_before_command'";
    }

    if (ConfigTools::readOptionalValue<std::chrono::milliseconds>(mDelayBeforeFirstCommand, data, "delay_before_first_poll")) {
        BOOST_LOG_SEV(log, Log::warn) << "'delay_before_first_poll' is deprecated and will be removed in future releases. Rename it to 'delay_before_first_command'";
    }

    ConfigTools::readOptionalValue<std::string>(this->mSlaveName, data, "name");

    ConfigTools::readOptionalValue<std::chrono::milliseconds>(this->mDelayBeforeCommand, data, "delay_before_command");
    ConfigTools::readOptionalValue<std::chrono::milliseconds>(this->mDelayBeforeFirstCommand, data, "delay_before_first_command");

    ConfigTools::readOptionalValue<unsigned short>(this->mMaxWriteRetryCount, data, "write_retries");
    ConfigTools::readOptionalValue<unsigned short>(this->mMaxReadRetryCount, data, "read_retries");
}

}
