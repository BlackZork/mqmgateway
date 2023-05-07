#include "config.hpp"
#include "common.hpp"

template<>
struct YAML::convert<modmqttd::ModbusNetworkConfig::RtuSerialMode> {
    static bool decode(const YAML::Node& node, modmqttd::ModbusNetworkConfig::RtuSerialMode& rhs) {
        auto str = node.as<std::string>();
        if (str == "rs232") {
            rhs = modmqttd::ModbusNetworkConfig::RtuSerialMode::RS232;
        } else if (str == "rs485") {
            rhs = modmqttd::ModbusNetworkConfig::RtuSerialMode::RS485;
        } else {
            return false;
        }
        return true;
    }
};

template<>
struct YAML::convert<modmqttd::ModbusNetworkConfig::RtuRtsMode> {
    static bool decode(const YAML::Node& node, modmqttd::ModbusNetworkConfig::RtuRtsMode& rhs) {
        auto str = node.as<std::string>();
        if (str == "down") {
            rhs = modmqttd::ModbusNetworkConfig::RtuRtsMode::DOWN;
        } else if (str == "up") {
            rhs = modmqttd::ModbusNetworkConfig::RtuRtsMode::UP;
        } else {
            return false;
        }
        return true;
    }
};

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
        mDataBit = ConfigTools::readRequiredValue<uint8_t>(source, "data_bit");
        mStopBit = ConfigTools::readRequiredValue<uint8_t>(source, "stop_bit");
        ConfigTools::readOptionalValue<RtuSerialMode>(this->mRtuSerialMode, source, "rtu_serial_mode");
        ConfigTools::readOptionalValue<RtuRtsMode>(this->mRtsMode, source, "rtu_rts_mode");
        ConfigTools::readOptionalValue<int>(this->mRtsDelayUs, source, "rtu_rts_delay");
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
