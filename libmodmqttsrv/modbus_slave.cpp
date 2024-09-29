#include "modbus_slave.hpp"
#include "yaml_converters.hpp"
#include "config.hpp"

namespace modmqttd {

boost::log::sources::severity_logger<Log::severity> ModbusSlaveConfig::log;

ModbusSlaveConfig::ModbusSlaveConfig(int pAddress, const YAML::Node& data)
    : mAddress(pAddress)
{
    ConfigTools::readOptionalValue<std::string>(mSlaveName, data, "name");

    std::chrono::milliseconds tmpval;
    if (ConfigTools::readOptionalValue<std::chrono::milliseconds>(tmpval, data, "delay_before_poll")) {
        BOOST_LOG_SEV(log, Log::warn) << "'delay_before_poll' is deprecated and will be removed in future releases. Rename it to 'delay_before_command'";
        setDelayBeforeCommand(tmpval);
    }

    if (ConfigTools::readOptionalValue<std::chrono::milliseconds>(tmpval, data, "delay_before_command")) {
        setDelayBeforeCommand(tmpval);
    }


    if (ConfigTools::readOptionalValue<std::chrono::milliseconds>(tmpval, data, "delay_before_first_poll")) {
        BOOST_LOG_SEV(log, Log::warn) << "'delay_before_first_poll' is deprecated and will be removed in future releases. Rename it to 'delay_before_first_command'";
        setDelayBeforeFirstCommand(tmpval);
    }

    if (ConfigTools::readOptionalValue<std::chrono::milliseconds>(tmpval, data, "delay_before_first_command")) {
        setDelayBeforeFirstCommand(tmpval);
    }

    ConfigTools::readOptionalValue<unsigned short>(mMaxWriteRetryCount, data, "write_retries");
    ConfigTools::readOptionalValue<unsigned short>(mMaxReadRetryCount, data, "read_retries");
}

}
