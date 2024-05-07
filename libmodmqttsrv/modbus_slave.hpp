#pragma once

#include <yaml-cpp/yaml.h>
#include <chrono>

#include "logging.hpp"

namespace modmqttd {

class ModbusSlaveConfig {

    static boost::log::sources::severity_logger<Log::severity> log;

    public:
        ModbusSlaveConfig(const YAML::Node& data);
        int mAddress;
        std::chrono::milliseconds mDelayBeforeCommand = std::chrono::milliseconds::zero();
        std::chrono::milliseconds mDelayBeforeFirstCommand = std::chrono::milliseconds::zero();

        unsigned short mMaxWriteRetryCount = 0;
        unsigned short mMaxReadRetryCount = 0;
};

}
