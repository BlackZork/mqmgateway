#include "config.hpp"
#include "common.hpp"

namespace modmqttd {


ConfigurationException::ConfigurationException(const YAML::Mark& mark, const char* what) {
    mWhat = "config error";
    if (mark.is_null()) {
        mWhat += ": ";
    } else {
        mWhat += "(line ";
        mWhat += std::to_string(mark.line);
        mWhat += "): ";
    }
    mWhat += what;
}

ModbusNetworkConfig::ModbusNetworkConfig(const YAML::Node& source) {
    mName = ConfigTools::readRequiredString(source, "name");

    if (source["device"]) {
        mType = Type::RTU;
        mDevice = ConfigTools::readRequiredString(source, "device");
        mBaud = ConfigTools::readRequiredValue<int>(source, "baud");
        mParity = ConfigTools::readRequiredValue<char>(source, "parity");
        mDataBit = ConfigTools::readRequiredValue<int>(source, "data_bit");
        mStopBit = ConfigTools::readRequiredValue<int>(source, "stop_bit");
    } else if (source["address"]) {
        mType = Type::TCPIP;
        mAddress = ConfigTools::readRequiredString(source, "address");
        mPort = ConfigTools::readRequiredValue<int>(source, "port");
    } else {
        throw ConfigurationException(source.Mark(), "Cannot determine modbus network type: missing 'device' or 'address'");
    }
}

MqttBrokerConfig::MqttBrokerConfig(const YAML::Node& source) {
    mHost = ConfigTools::readRequiredString(source, "host");
    ConfigTools::readOptionalValue<int>(mPort, source, "port");
    ConfigTools::readOptionalValue<int>(mKeepalive, source, "keepalive");
    ConfigTools::readOptionalValue<std::string>(mUsername, source, "username");
    ConfigTools::readOptionalValue<std::string>(mPassword, source, "password");
}


}
