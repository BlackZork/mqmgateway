#include <boost/filesystem.hpp>
#include "config.hpp"
#include "common.hpp"
#include "yaml_converters.hpp"

namespace fs = boost::filesystem;

namespace modmqttd {

boost::log::sources::severity_logger<Log::severity> ModbusNetworkConfig::log;

#if __cplusplus < 201703L
constexpr std::chrono::milliseconds ModbusNetworkConfig::MAX_RESPONSE_TIMEOUT;
#endif

ConfigurationException::ConfigurationException(const YAML::Mark& mark, const char* what) {
    mWhat = "config error";
    if (mark.is_null()) {
        mWhat += ": ";
    } else {
        mLineNumber = mark.line+1;
        mWhat += "(line ";
        mWhat += std::to_string(mLineNumber);
        mWhat += "): ";
    }
    mWhat += what;
}

ModbusNetworkConfig::ModbusNetworkConfig(const YAML::Node& source) {
    mName = ConfigTools::readRequiredString(source, "name");

    YAML::Node rtNode(ConfigTools::setOptionalValueFromNode<std::chrono::milliseconds>(mResponseTimeout, source, "response_timeout"));
    if (rtNode.IsDefined()) {
        if ((mResponseTimeout < std::chrono::milliseconds::zero()) || (mResponseTimeout > MAX_RESPONSE_TIMEOUT))
            throw ConfigurationException(rtNode.Mark(), "response_timeout value must be in range 0-999ms");
    }

    YAML::Node rtdNode(ConfigTools::setOptionalValueFromNode<std::chrono::milliseconds>(mResponseDataTimeout, source, "response_data_timeout"));
    if (rtdNode.IsDefined()) {
        if ((mResponseDataTimeout < std::chrono::milliseconds::zero()) || (mResponseDataTimeout > MAX_RESPONSE_TIMEOUT))
            throw ConfigurationException(rtdNode.Mark(), "response_data_timeout value must be in range 0-999ms");
    }

    std::chrono::milliseconds tmpval;
    if (ConfigTools::readOptionalValue<std::chrono::milliseconds>(tmpval, source, "min_delay_before_poll")) {
        BOOST_LOG_SEV(log, Log::warn) << "'min_delay_before_poll' is deprecated and will be removed in future releases. Rename it to 'delay_before_command'";
        setDelayBeforeCommand(tmpval);
    }

    if (ConfigTools::readOptionalValue<std::chrono::milliseconds>(tmpval, source, "delay_before_command")) {
        setDelayBeforeCommand(tmpval);
    }

    if (ConfigTools::readOptionalValue<std::chrono::milliseconds>(tmpval, source, "delay_before_first_command")) {
        setDelayBeforeFirstCommand(tmpval);
    }

    ConfigTools::readOptionalValue<unsigned short>(mMaxWriteRetryCount, source, "write_retries");
    ConfigTools::readOptionalValue<unsigned short>(mMaxReadRetryCount, source, "read_retries");


    if (source["device"]) {
        mType = Type::RTU;
        mDevice = ConfigTools::readRequiredString(source, "device");
        mBaud = ConfigTools::readRequiredValue<int>(source, "baud");
        mParity = ConfigTools::readRequiredValue<char>(source, "parity");
        mDataBit = ConfigTools::readRequiredValue<int>(source, "data_bit");
        mStopBit = ConfigTools::readRequiredValue<int>(source, "stop_bit");
        ConfigTools::readOptionalValue<RtuSerialMode>(mRtuSerialMode, source, "rtu_serial_mode");
        ConfigTools::readOptionalValue<RtuRtsMode>(mRtsMode, source, "rtu_rts_mode");
        ConfigTools::readOptionalValue<int>(mRtsDelayUs, source, "rtu_rts_delay_us");

        mWatchdogConfig.mDevicePath = mDevice;
    } else if (source["address"]) {
        mType = Type::TCPIP;
        mAddress = ConfigTools::readRequiredString(source, "address");
        mPort = ConfigTools::readRequiredValue<int>(source, "port");
    } else {
        throw ConfigurationException(source.Mark(), "Cannot determine modbus network type: missing 'device' or 'address'");
    }

    if (source["watchdog"]) {
        ConfigTools::readOptionalValue<std::chrono::milliseconds>(mWatchdogConfig.mWatchPeriod, source["watchdog"], "watch_period");
    }
}

MqttBrokerConfig::MqttBrokerConfig(const YAML::Node& source) {
    mHost = ConfigTools::readRequiredString(source, "host");
    if (source["tls"]) {
        mTLS = true;
        mPort = 8883;
        YAML::Node cafileNode(ConfigTools::setOptionalValueFromNode<std::string>(mCafile, source["tls"], "cafile"));
        if (cafileNode.IsDefined()) {
            fs::path filePath(mCafile);
            if (!fs::exists(filePath) || fs::is_directory(filePath)) {
                throw ConfigurationException(cafileNode.Mark(), "CA file '" + mCafile + "' is not a readable file");
            }
        }
    }
    ConfigTools::readOptionalValue<int>(mPort, source, "port");
    ConfigTools::readOptionalValue<int>(mKeepalive, source, "keepalive");
    ConfigTools::readOptionalValue<std::string>(mUsername, source, "username");
    ConfigTools::readOptionalValue<std::string>(mPassword, source, "password");
}


}
