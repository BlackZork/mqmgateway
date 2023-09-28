#pragma once

#include <chrono>
#include <regex>

#include <yaml-cpp/yaml.h>
#include "libmodmqttsrv/exceptions.hpp"
#include "libmodmqttsrv/config.hpp"

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

template<>
struct YAML::convert<std::chrono::milliseconds> {
    static bool decode(const YAML::Node& node, std::chrono::milliseconds& value) {
        const std::regex re("([0-9]+)(ms|s|min)");
        std::cmatch matches;
        std::string strval = node.as<std::string>();

        if (!std::regex_match(strval.c_str(), matches, re))
            throw modmqttd::ConfigurationException(node.Mark(), "Invalid time specification");

        int mval = std::stoi(matches[1]);
        std::string unit = matches[2];
        if (unit == "s")
            mval *= 1000;
        else if (unit == "min")
            mval *= 1000 * 60;

        value = std::chrono::milliseconds(mval);
        return true;
    }
};
