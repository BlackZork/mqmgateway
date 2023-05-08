#pragma once

#include <modbus/modbus.h>

#include "config.hpp"
#include "modbus_messages.hpp"
#include "logging.hpp"
#include "register_poll.hpp"
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
        virtual uint16_t readModbusRegister(int slaveId, const RegisterPoll& regData);
        virtual std::vector<uint16_t> readModbusRegisters(const MsgRegisterReadRemoteCall& msg);
        virtual void writeModbusRegister(const MsgRegisterValue& msg);
        virtual void writeModbusRegisters(const MsgRegisterWriteRemoteCall& msg);
        virtual ~ModbusContext() {
            modbus_free(mCtx);
        };
    private:
        boost::log::sources::severity_logger<Log::severity> log;
        void handleError(const std::string& desc);
        bool mIsConnected = false;
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
