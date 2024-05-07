#pragma once

#include "imodbuscontext.hpp"
#include "modbus_messages.hpp"
#include "register_poll.hpp"

namespace modmqttd {

class ModbusWatchdog : IModbusWatchdog {
    public:
        void init(const ModbusWatchdogConfig& conf);

        void inspectCommand(const RegisterCommand& command);
        void reset();
        bool isReconnectRequired() const;
        std::chrono::steady_clock::time_point getLastSuccessfulCommandTime() const;
        std::chrono::steady_clock::duration getCurrentErrorPeriod() const {
            return std::chrono::steady_clock::now() - mLastSuccessfulCommandTime;
        }

    private:
        static boost::log::sources::severity_logger<Log::severity> log;

        ModbusWatchdogConfig mConfig;
        std::chrono::steady_clock::time_point mLastSuccessfulCommandTime;

};


}
