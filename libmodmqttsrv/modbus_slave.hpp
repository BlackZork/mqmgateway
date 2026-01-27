#pragma once

#include <yaml-cpp/yaml.h>
#include <chrono>

#include "logging.hpp"

namespace modmqttd {

class ModbusSlaveConfig {
    public:
        ModbusSlaveConfig(int pAddress, const YAML::Node& data);
        int mAddress;
        std::string mSlaveName;

        bool hasDelayBeforeCommand() const { return mDelayBeforeCommand != nullptr; }
        bool hasDelayBeforeFirstCommand() const { return mDelayBeforeFirstCommand != nullptr; }

        const std::shared_ptr<std::chrono::milliseconds>& getDelayBeforeCommand() const { return mDelayBeforeCommand; }
        const std::shared_ptr<std::chrono::milliseconds>& getDelayBeforeFirstCommand() const { return mDelayBeforeFirstCommand; }

        void setDelayBeforeCommand(const std::chrono::milliseconds& pDelay) { mDelayBeforeCommand.reset(new std::chrono::milliseconds(pDelay)); }
        void setDelayBeforeFirstCommand(const std::chrono::milliseconds& pDelay) { mDelayBeforeFirstCommand.reset(new std::chrono::milliseconds(pDelay)); }


        unsigned short mMaxWriteRetryCount = 0;
        unsigned short mMaxReadRetryCount = 0;
    private:
        std::shared_ptr<std::chrono::milliseconds> mDelayBeforeCommand;
        std::shared_ptr<std::chrono::milliseconds> mDelayBeforeFirstCommand;
};

}
