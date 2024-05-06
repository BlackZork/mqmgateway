#pragma once

#include "imodbuscontext.hpp"
#include "modbus_messages.hpp"

namespace modmqttd {

class ModbusWatchdog : IModbusWatchdog {
    public:
        void init(const ModbusWatchdogConfig& conf, const std::shared_ptr<IModbusContext>& modbus)

        void inspectCommand(const IRegisterCommand& command);
        void reset();
        bool isReconnectRequired() const;

    private:
        static  boost::log::sources::severity_logger<Log::severity> log;

        std::shared_ptr<IModbusContext> mModbus;

        ModbusWatchdogConfig mConfig;
        std::chrono::steady_clock::time_point mLastSuccessfulCommandTime;

};


}