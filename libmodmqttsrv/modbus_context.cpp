#include "modbus_context.hpp"

namespace modmqttd {

void
ModbusContext::init(const ModbusNetworkConfig& config)
{
    if (config.mType == ModbusNetworkConfig::TCPIP) {
        BOOST_LOG_SEV(log, Log::info) << "Connecting to " << config.mAddress << ":" << config.mPort;
        mCtx = modbus_new_tcp(config.mAddress.c_str(), config.mPort);
        modbus_set_error_recovery(mCtx,
            (modbus_error_recovery_mode)
                (MODBUS_ERROR_RECOVERY_PROTOCOL|MODBUS_ERROR_RECOVERY_LINK)
        );
    } else {
        mCtx = modbus_new_rtu(
            config.mDevice.c_str(),
            config.mBaud,
            config.mParity,
            config.mDataBit,
            config.mStopBit
        );
        modbus_set_error_recovery(mCtx, MODBUS_ERROR_RECOVERY_PROTOCOL);
    }

    if (mCtx == NULL)
        throw ModbusContextException("Unable to create context");
};

void
ModbusContext::connect() {
    if (mCtx != nullptr)
        modbus_close(mCtx);

    if (modbus_connect(mCtx) == -1) {
        BOOST_LOG_SEV(log, Log::error) << "modbus connection failed("<< errno << ") : " << modbus_strerror(errno);
        mIsConnected = false;
    } else {
        mIsConnected = true;
    }
}

void
ModbusContext::disconnect() {
    modbus_close(mCtx);
}

uint16_t
ModbusContext::readModbusRegister(int slaveId, const RegisterPoll& regData) {
    if (slaveId != 0)
        modbus_set_slave(mCtx, slaveId);
    else
        modbus_set_slave(mCtx, MODBUS_TCP_SLAVE);

    uint8_t bits;
    uint16_t value;
    int retCode;
    switch(regData.mRegisterType) {
        case RegisterType::COIL:
            retCode = modbus_read_bits(mCtx, regData.mRegisterAddress, 1, &bits);
            value = bits;
        break;
        case RegisterType::BIT:
            retCode = modbus_read_input_bits(mCtx, regData.mRegisterAddress, 1, &bits);
            value = bits;
        break;
        case RegisterType::HOLDING:
            retCode = modbus_read_registers(mCtx, regData.mRegisterAddress, 1, &value);
        break;
        case RegisterType::INPUT:
            retCode = modbus_read_input_registers(mCtx, regData.mRegisterAddress, 1, &value);
        break;
        default:
            throw ModbusContextException(std::string("Cannot read, unknown register type ") + std::to_string(regData.mRegisterType));
    }
    if (retCode == -1)
        throw ModbusReadException(std::string("read fn ") + std::to_string(regData.mRegisterAddress) + " failed");

    return value;
}

void
ModbusContext::writeModbusRegister(const MsgRegisterValue& msg) {
    if (msg.mSlaveId != 0)
        modbus_set_slave(mCtx, msg.mSlaveId);
    else
        modbus_set_slave(mCtx, MODBUS_TCP_SLAVE);

    int retCode;
    switch(msg.mRegisterType) {
        case RegisterType::COIL:
            retCode = modbus_write_bit(mCtx, msg.mRegisterAddress, msg.mValue == 1 ? TRUE : FALSE);
        break;
        case RegisterType::HOLDING:
            retCode = modbus_write_register(mCtx, msg.mRegisterAddress, msg.mValue);
        break;
        default:
            throw ModbusContextException(std::string("Cannot write, unknown register type ") + std::to_string(msg.mRegisterType));
    }
    if (retCode == -1)
        throw ModbusWriteException(std::string("write fn ") + std::to_string(msg.mRegisterAddress) + " failed");
}

} //namespace
