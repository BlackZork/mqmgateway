#pragma once

#include <inttypes.h>
#include <memory>

#include "config.hpp"

namespace modmqttd {

class RegisterPoll;
class MsgRegisterValues;

/**
    Abstract base class for modbus communication library implementation
*/
class IModbusContext {
    public:
        virtual void init(const ModbusNetworkConfig& config) = 0;
        virtual void connect() = 0;
        virtual bool isConnected() const = 0;
        virtual void disconnect() = 0;
        virtual std::vector<uint16_t> readModbusRegisters(int slaveId, const RegisterPoll& regData) = 0;
        virtual void writeModbusRegisters(const MsgRegisterValues& msg) = 0;
        virtual ModbusNetworkConfig::Type getNetworkType() const = 0;
        virtual ~IModbusContext() {};
};

class IModbusFactory {
    public:
        virtual std::shared_ptr<IModbusContext> getContext(const std::string& networkName) = 0;
        virtual ~IModbusFactory() {};
};

}
