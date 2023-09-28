#include "modbus_context.hpp"

namespace modmqttd {

void
ModbusContext::init(const ModbusNetworkConfig& config)
{
    mNetworkType = config.mType;
    if (mNetworkType == ModbusNetworkConfig::TCPIP) {
        BOOST_LOG_SEV(log, Log::info) << "Connecting to " << config.mAddress << ":" << config.mPort;
        mCtx = modbus_new_tcp(config.mAddress.c_str(), config.mPort);
        modbus_set_error_recovery(mCtx,
            (modbus_error_recovery_mode)
                (MODBUS_ERROR_RECOVERY_PROTOCOL|MODBUS_ERROR_RECOVERY_LINK)
        );
    } else {
        BOOST_LOG_SEV(log, Log::info) << "Creating RTU context: " << config.mDevice << ", " << config.mBaud << "-" << config.mDataBit << config.mParity << config.mStopBit;
        mCtx = modbus_new_rtu(
            config.mDevice.c_str(),
            config.mBaud,
            config.mParity,
            config.mDataBit,
            config.mStopBit
        );
        modbus_set_error_recovery(mCtx, MODBUS_ERROR_RECOVERY_PROTOCOL);

        int serialMode;
        std::string serialModeStr;
        switch (config.mRtuSerialMode) {
            case ModbusNetworkConfig::RtuSerialMode::RS232:
                serialMode = MODBUS_RTU_RS232;
                serialModeStr = "RS232";
                break;
            case ModbusNetworkConfig::RtuSerialMode::RS485:
                serialMode = MODBUS_RTU_RS485;
                serialModeStr = "RS485";
                break;
            case ModbusNetworkConfig::RtuSerialMode::UNSPECIFIED:
            default:
                serialMode = -1;
                break;
        }
        if (serialMode >= 0) {
            if (modbus_rtu_set_serial_mode(mCtx, serialMode)) {
                throw ModbusContextException("Unable to set RTU serial mode");
            }
            BOOST_LOG_SEV(log, Log::info) << "RTU serial mode set to " << serialModeStr;
        }

        int rtsMode;
        std::string rtsModeStr;
        switch (config.mRtsMode) {
            case ModbusNetworkConfig::RtuRtsMode::UP:
                rtsMode = MODBUS_RTU_RTS_UP;
                rtsModeStr = "UP";
                break;
            case ModbusNetworkConfig::RtuRtsMode::DOWN:
                rtsMode = MODBUS_RTU_RTS_DOWN;
                rtsModeStr = "DOWN";
                break;
            case ModbusNetworkConfig::RtuRtsMode::NONE:
            default:
                rtsMode = -1;
                break;
        }
        if (rtsMode >= 0) {
            if (modbus_rtu_set_rts(mCtx, rtsMode)) {
                throw ModbusContextException("Unable to set RTS mode");
            }
            BOOST_LOG_SEV(log, Log::info) << "RTU RTS mode set to " << rtsModeStr;
        }

        if (config.mRtsDelayUs > 0) {
            if (modbus_rtu_set_rts_delay(mCtx, config.mRtsDelayUs)) {
                throw ModbusContextException("Unable to set RTS delay");
            }
            BOOST_LOG_SEV(log, Log::info) << "RTU delay set to " << config.mRtsDelayUs << "us";
        }


        uint32_t us = std::chrono::duration_cast<std::chrono::microseconds>(config.mResponseTimeout).count();
        if (modbus_set_response_timeout(mCtx, 0, us)) {
            throw ModbusContextException("Unable to set response timeout");
        }
        BOOST_LOG_SEV(log, Log::info) << "Response timeout set to " << config.mResponseTimeout.count() << "ms";

        if (config.mResponseDataTimeout.count() > 0) {
            us = std::chrono::duration_cast<std::chrono::microseconds>(config.mResponseDataTimeout).count();
            if (modbus_set_byte_timeout(mCtx, 0, us)) {
                throw ModbusContextException("Unable to set response timeout");
            }
            BOOST_LOG_SEV(log, Log::info) << "Data response timeout set to " << config.mResponseDataTimeout.count() << "ms";
        }
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

std::vector<uint16_t>
ModbusContext::readModbusRegisters(int slaveId, const RegisterPoll& regData) {
    if (slaveId != 0)
        modbus_set_slave(mCtx, slaveId);
    else
        modbus_set_slave(mCtx, MODBUS_TCP_SLAVE);

    uint8_t bits;
    std::vector<uint16_t> ret(regData.getCount(), 0);
    int arraySize = 0;
    int retCode;
    switch(regData.mRegisterType) {
        //for COIL and BIT store bits in uint16_t array
        case RegisterType::COIL: {
            arraySize = regData.getCount();
            std::shared_ptr<uint8_t> values((uint8_t*)malloc(arraySize), free);
            std::memset(values.get(), 0x0, arraySize);
            retCode = modbus_read_bits(mCtx, regData.mRegister, regData.getCount(), (uint8_t*)values.get());
            for(int i = 0; i < arraySize; i++)
                ret[i] = values.get()[i];
        } break;
        case RegisterType::BIT: {
            arraySize = regData.getCount();
            std::shared_ptr<uint8_t> values((uint8_t*)malloc(arraySize), free);
            std::memset(values.get(), 0x0, arraySize);
            retCode = modbus_read_input_bits(mCtx, regData.mRegister, regData.getCount(), (uint8_t*)values.get());
            for(int i = 0; i < arraySize; i++)
                ret[i] = values.get()[i];
        } break;
        case RegisterType::HOLDING: {
            arraySize = regData.getCount();
            std::shared_ptr<uint16_t> values((uint16_t*)malloc(sizeof(uint16_t) * arraySize), free);
            retCode = modbus_read_registers(mCtx, regData.mRegister, regData.getCount(), values.get());
            for(int i = 0; i < arraySize; i++)
                ret[i] = values.get()[i];
        } break;
        case RegisterType::INPUT: {
            arraySize = regData.getCount();
            std::shared_ptr<uint16_t> values((uint16_t*)malloc(sizeof(uint16_t) * arraySize), free);
            retCode = modbus_read_input_registers(mCtx, regData.mRegister, regData.getCount(), values.get());
            for(int i = 0; i < arraySize; i++)
                ret[i] = values.get()[i];
        } break;
        default:
            throw ModbusContextException(std::string("Cannot read, unknown register type ") + std::to_string(regData.mRegisterType));
    }
    if (retCode == -1)
        throw ModbusReadException(std::string("read fn ") + std::to_string(regData.mRegister) + std::string(" failed with return code ") + std::to_string(retCode));

    return ret;
}

void
ModbusContext::writeModbusRegisters(const MsgRegisterValues& msg) {
    if (msg.mSlaveId != 0)
        modbus_set_slave(mCtx, msg.mSlaveId);
    else
        modbus_set_slave(mCtx, MODBUS_TCP_SLAVE);


    int retCode;
    switch(msg.mRegisterType) {
        case RegisterType::COIL:
            if (msg.mRegisters.getCount() == 1) {
                uint16_t value = msg.mRegisters.getValue(0);
                retCode = modbus_write_bit(mCtx, msg.mRegisterNumber, value == 1 ? TRUE : FALSE);
            } else {
                int arraySize = msg.mRegisters.getCount();
                std::shared_ptr<uint8_t> values((uint8_t*)malloc(arraySize), free);
                std::memset(values.get(), 0x0, arraySize);
                for(int i = 0; i < arraySize; i++) {
                    uint16_t value = msg.mRegisters.getValue(i);
                    values.get()[i] = value == 1 ? TRUE : FALSE;
                }
                retCode = modbus_write_bits(mCtx, msg.mRegisterNumber, msg.mRegisters.getCount(), values.get());
            }
        break;
        case RegisterType::HOLDING:
            if (msg.mRegisters.getCount() == 1) {
                retCode = modbus_write_register(mCtx, msg.mRegisterNumber, msg.mRegisters.getValue(0));
            } else {
                int elSize = sizeof(uint16_t);
                int arraySize = msg.mRegisters.getCount();
                size_t bufSize = elSize * arraySize;
                std::shared_ptr<uint16_t> values((uint16_t*)malloc(bufSize), free);
                std::memset(values.get(), 0x0, bufSize);
                for(int i = 0; i < msg.mRegisters.getCount(); i++) {
                    values.get()[i] = msg.mRegisters.getValue(i);
                }
                retCode = modbus_write_registers(mCtx, msg.mRegisterNumber, msg.mRegisters.getCount(), values.get());
            }
        break;
        default:
            throw ModbusContextException(std::string("Cannot write, unknown register type ") + std::to_string(msg.mRegisterType));
    }
    if (retCode == -1)
        throw ModbusWriteException(std::string("write fn ") + std::to_string(msg.mRegisterNumber) + " failed");
}

} //namespace
