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
            BOOST_LOG_SEV(log, Log::info) << "Set RTU serial mode to " << serialModeStr;
            if (modbus_rtu_set_serial_mode(mCtx, serialMode)) {
                throw ModbusContextException("Unable to set RTU serial mode");
            }
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
            BOOST_LOG_SEV(log, Log::info) << "Set RTU RTS mode to " << rtsModeStr;
            if (modbus_rtu_set_rts(mCtx, rtsMode)) {
                throw ModbusContextException("Unable to set RTS mode");
            }
        }

        if (config.mRtsDelayUs > 0) {
            BOOST_LOG_SEV(log, Log::info) << "Set RTU delay to " << config.mRtsDelayUs << "us";
            if (modbus_rtu_set_rts_delay(mCtx, config.mRtsDelayUs)) {
                throw ModbusContextException("Unable to set RTS delay");
            }
        }

        BOOST_LOG_SEV(log, Log::info) << "Set RTU response timeout to 1s";
        if (modbus_set_response_timeout(mCtx, 1, 0)) {
            throw ModbusContextException("Unable to set response timeout");
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
    std::shared_ptr<uint16_t> values;
    int arraySize = 0;
    int retCode;
    switch(regData.mRegisterType) {
        //for COIL and BIT store bits in uint16_t array
        case RegisterType::COIL: {
            arraySize = regData.getCount()/16 + 1;
            size_t bufSize = sizeof(uint16_t) * arraySize;
            values.reset((uint16_t*)malloc(bufSize), free);
            std::memset(values.get(), 0x0, bufSize);
            retCode = modbus_read_bits(mCtx, regData.mRegister, regData.getCount(), (uint8_t*)values.get());
        } break;
        case RegisterType::BIT: {
            arraySize = regData.getCount()/16 + 1;
            size_t bufSize = sizeof(uint16_t) * arraySize;
            values.reset((uint16_t*)malloc(bufSize), free);
            std::memset(values.get(), 0x0, bufSize);
            retCode = modbus_read_input_bits(mCtx, regData.mRegister, regData.getCount(), (uint8_t*)values.get());
        } break;
        case RegisterType::HOLDING:
            arraySize = regData.getCount();
            values.reset((uint16_t*)malloc(sizeof(uint16_t) * arraySize), free);
            retCode = modbus_read_registers(mCtx, regData.mRegister, regData.getCount(), values.get());
        break;
        case RegisterType::INPUT:
            arraySize = regData.getCount();
            values.reset((uint16_t*)malloc(sizeof(uint16_t) * arraySize), free);
            retCode = modbus_read_input_registers(mCtx, regData.mRegister, regData.getCount(), values.get());
        break;
        default:
            throw ModbusContextException(std::string("Cannot read, unknown register type ") + std::to_string(regData.mRegisterType));
    }
    if (retCode == -1)
        throw ModbusReadException(std::string("read fn ") + std::to_string(regData.mRegister) + std::string(" failed with return code ") + std::to_string(retCode));

    std::vector<uint16_t> ret(values.get(), values.get() + arraySize);

    return ret;
}

void
ModbusContext::writeModbusRegisters(const MsgRegisterValues& msg) {
    if (msg.mSlaveId != 0)
        modbus_set_slave(mCtx, msg.mSlaveId);
    else
        modbus_set_slave(mCtx, MODBUS_TCP_SLAVE);

    u_int16_t value = msg.mRegisters.getValue(0);

    int retCode;
    switch(msg.mRegisterType) {
        case RegisterType::COIL:
            retCode = modbus_write_bit(mCtx, msg.mRegisterNumber, value == 1 ? TRUE : FALSE);
        break;
        case RegisterType::HOLDING:
            retCode = modbus_write_register(mCtx, msg.mRegisterNumber, value);
        break;
        default:
            throw ModbusContextException(std::string("Cannot write, unknown register type ") + std::to_string(msg.mRegisterType));
    }
    if (retCode == -1)
        throw ModbusWriteException(std::string("write fn ") + std::to_string(msg.mRegisterNumber) + " failed");
}

} //namespace
