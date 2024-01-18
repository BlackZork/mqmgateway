#pragma once

#include <yaml-cpp/yaml.h>

namespace modmqttd {

class ModbusSlaveConfig {
    public:
        ModbusSlaveConfig(const YAML::Node& data);
        int mAddress;
};

}
