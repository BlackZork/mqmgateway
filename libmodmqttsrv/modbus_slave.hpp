#pragma once

#include <yaml-cpp/yaml.h>
#include <chrono>

namespace modmqttd {

class ModbusSlaveConfig {
    public:
        ModbusSlaveConfig(const YAML::Node& data);
        int mAddress;
        std::chrono::milliseconds mDelayBeforePoll = std::chrono::milliseconds::zero();
};

}
