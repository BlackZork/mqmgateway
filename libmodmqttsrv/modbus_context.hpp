#pragma once

#include <modbus/modbus.h>

#include "modbus_messages.hpp"
#include "logging.hpp"
#include "imodbuscontext.hpp"

namespace modmqttd {

/**
 * Wrapper for libmodbus
 * */
class ModbusContext : public IModbusContext {
    public:
        virtual void init(const ModbusNetworkConfig& config);
        virtual void connect();
        virtual bool isConnected() const { return mIsConnected; }
        virtual void disconnect();
        virtual std::vector<uint16_t> readModbusRegisters(int slaveId, const RegisterPoll& regData);
        virtual void writeModbusRegisters(int slaveId, const RegisterWrite& msg);
        virtual ModbusNetworkConfig::Type getNetworkType() const { return mNetworkType; }
        virtual ~ModbusContext() {
            modbus_free(mCtx);
        };
    private:
        void handleError(const std::string& desc);
        bool mIsConnected = false;
        ModbusNetworkConfig::Type mNetworkType;
        std::string mNetworkAddress;
        modbus_t* mCtx = NULL;
};

class ModbusFactory : public IModbusFactory {
    public:
        virtual std::shared_ptr<IModbusContext> getContext(const std::string& networkName) {
            return std::shared_ptr<IModbusContext>(new ModbusContext());
        }
};

class ModbusContextException : public ModMqttException {
    public:
        ModbusContextException(const std::string& what) {
            mWhat = std::string("libmodbus: ") + what + ": " + modbus_strerror(errno);
        }
};

class ModbusReadException : public ModbusContextException {
    public:
        ModbusReadException(const std::string& what) : ModbusContextException(what) {}
};

class ModbusWriteException : public ModbusContextException {
    public:
        ModbusWriteException(const std::string& what) : ModbusContextException(what) {}
};


} //namespace
