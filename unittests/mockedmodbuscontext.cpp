#include "mockedmodbuscontext.hpp"

#include "libmodmqttsrv/modbus_messages.hpp"
#include "libmodmqttsrv/modbus_context.hpp"
#include "libmodmqttsrv/debugtools.hpp"
#include "catch2/catch.hpp"

#include <thread>
#include <iostream>

const std::chrono::milliseconds MockedModbusContext::sDefaultSlaveReadTime = std::chrono::milliseconds(5);
const std::chrono::milliseconds MockedModbusContext::sDefaultSlaveWriteTime = std::chrono::milliseconds(10);

void
MockedModbusContext::Slave::write(const modmqttd::MsgRegisterValues& msg, bool internalOperation) {
    if (!internalOperation) {
        std::this_thread::sleep_for(mWriteTime);
        if (mDisconnected) {
            errno = EIO;
            throw modmqttd::ModbusWriteException(std::string("write fn ") + std::to_string(msg.mRegisterNumber) + " failed");
        }
        if (hasError(msg.mRegisterNumber, msg.mRegisterType)) {
            errno = EIO;
            throw modmqttd::ModbusReadException(std::string("register write fn ") + std::to_string(msg.mRegisterNumber) + " failed");
        }
    }

    u_int16_t value = msg.mValues.getValue(0);

    switch(msg.mRegisterType) {
        case modmqttd::RegisterType::COIL:
            mCoil[msg.mRegisterNumber].mValue = value == 1;
        break;
        case modmqttd::RegisterType::BIT:
            mBit[msg.mRegisterNumber].mValue = value == 1;
        break;
        case modmqttd::RegisterType::HOLDING:
            mHolding[msg.mRegisterNumber].mValue = value;
        break;
        case modmqttd::RegisterType::INPUT:
            mInput[msg.mRegisterNumber].mValue = value;
        break;
        default:
            throw modmqttd::ModbusWriteException(std::string("Cannot write, unknown register type ") + std::to_string(msg.mRegisterType));
    };
    mWriteCount++;

}

std::vector<uint16_t>
MockedModbusContext::Slave::read(const modmqttd::RegisterPoll& regData, bool internalOperation) {
    if (!internalOperation) {
        std::this_thread::sleep_for(mReadTime);
        if (mDisconnected) {
            errno = EIO;
            throw modmqttd::ModbusReadException(std::string("read fn ") + std::to_string(regData.mRegister) + " failed");
        }
        if (hasError(regData.mRegister, regData.mRegisterType)) {
            errno = EIO;
            throw modmqttd::ModbusReadException(std::string("register read fn ") + std::to_string(regData.mRegister) + " failed");
        }
    }
    switch(regData.mRegisterType) {
        case modmqttd::RegisterType::COIL:
            return readRegisters(mCoil, regData.mRegister, regData.getCount());
        break;
        case modmqttd::RegisterType::HOLDING:
            return readRegisters(mHolding, regData.mRegister, regData.getCount());
        break;
        case modmqttd::RegisterType::INPUT:
            return readRegisters(mInput, regData.mRegister, regData.getCount());
        break;
        case modmqttd::RegisterType::BIT:
            return readRegisters(mBit, regData.mRegister, regData.getCount());
        break;
        default:
            throw modmqttd::ModbusReadException(std::string("Cannot read, unknown register type ") + std::to_string(regData.mRegisterType));
    };
}

bool
MockedModbusContext::Slave::hasError(int regNum, modmqttd::RegisterType regType) const {
    switch(regType) {
        case modmqttd::RegisterType::COIL:
            return hasError(mCoil, regNum);
        break;
        case modmqttd::RegisterType::HOLDING:
            return hasError(mHolding, regNum);
        break;
        case modmqttd::RegisterType::INPUT:
            return hasError(mInput, regNum);
        break;
        case modmqttd::RegisterType::BIT:
            return hasError(mBit, regNum);
        break;
        default:
            throw modmqttd::ModbusReadException(
                std::string("Cannot check for error, unknown register type ")
                + std::to_string(regType)
            );
    };
}

void
MockedModbusContext::Slave::setError(int regNum, modmqttd::RegisterType regType, bool pFlag) {
    switch(regType) {
        case modmqttd::RegisterType::COIL:
            mCoil[regNum].mError = pFlag;
        break;
        case modmqttd::RegisterType::BIT:
            mBit[regNum].mError = pFlag;
        break;
        case modmqttd::RegisterType::HOLDING:
            mHolding[regNum].mError = pFlag;
        break;
        case modmqttd::RegisterType::INPUT:
            mInput[regNum].mError = pFlag;
        break;
        default:
            throw modmqttd::ModbusReadException(std::string("Cannot set error, unknown register type ") + std::to_string(regType));
    };
}

std::vector<uint16_t>
MockedModbusContext::Slave::readRegisters(std::map<int, MockedModbusContext::Slave::RegData>& table, int num, int count) {
    mReadCount++;
    std::vector<uint16_t> ret;
    ret.reserve(count);
    for (int i = num; i < num + count; i++) {
        ret.push_back(readRegister(table, num));
    };
    return ret;
}

uint16_t
MockedModbusContext::Slave::readRegister(std::map<int, MockedModbusContext::Slave::RegData>& table, int num) {
    auto it = table.find(num);
    if (it == table.end())
        return 0;

    return it->second.mValue;
}

bool
MockedModbusContext::Slave::hasError(const std::map<int, MockedModbusContext::Slave::RegData>& table, int num) const {
    auto it = table.find(num);
    if (it == table.end())
        return false;
    return it->second.mError;
}

std::vector<uint16_t>
MockedModbusContext::readModbusRegisters(int slaveId, const modmqttd::RegisterPoll& regData) {
    std::unique_lock<std::mutex> lck(mMutex);
    std::map<int, Slave>::iterator it = findOrCreateSlave(slaveId);
    std::vector<uint16_t> data(it->second.read(regData, mInternalOperation));

    std::vector<uint16_t> ret;
    switch(regData.mRegisterType) {
        case modmqttd::RegisterType::COIL:
        case modmqttd::RegisterType::BIT: {
            int toSet = 0;
            uint16_t retval = 0;
            for(auto &val: data) {
                if (val) {
                    retval |= (1 << toSet);
                }
                toSet++;
                if (toSet == 16) {
                    toSet = 0;
                    ret.push_back(retval);
                    retval = 0;
                }
            }
            if (toSet != 16)
                ret.push_back(retval);
        } break;
        case modmqttd::RegisterType::HOLDING:
        case modmqttd::RegisterType::INPUT:
            ret = data;
        break;
    };


    if (mInternalOperation)
        BOOST_LOG_SEV(log, modmqttd::Log::info) << "MODBUS: " << mNetworkName
            << "." << it->second.mId << "." << regData.mRegister
            << " READED: " << modmqttd::DebugTools::registersToStr(ret);

    mInternalOperation = false;
    return ret;
}

void
MockedModbusContext::init(const modmqttd::ModbusNetworkConfig& config) {
    mNetworkName = config.mName;
}

void
MockedModbusContext::writeModbusRegisters(const modmqttd::MsgRegisterValues& msg) {
    std::unique_lock<std::mutex> lck(mMutex);
    std::map<int, Slave>::iterator it = findOrCreateSlave(msg.mSlaveId);

    //TODO write multiple registers at once
    u_int16_t value = msg.mValues.getValue(0);

    if (mInternalOperation)
        BOOST_LOG_SEV(log, modmqttd::Log::info) << "MODBUS: " << mNetworkName
            << "." << it->second.mId << "." << msg.mRegisterNumber
            << " WRITE: " << value;
    it->second.write(msg, mInternalOperation);
    mInternalOperation = false;
}

std::map<int, MockedModbusContext::Slave>::iterator
MockedModbusContext::findOrCreateSlave(int id) {
    std::map<int, Slave>::iterator it = mSlaves.find(id);
    if (it == mSlaves.end()) {
        Slave s(id);
        mSlaves[id] = s;
        it = mSlaves.find(id);
    }
    return it;
}

MockedModbusContext::Slave&
MockedModbusContext::getSlave(int slaveId) {
    return findOrCreateSlave(slaveId)->second;
}

int
MockedModbusContext::getReadCount(int slaveId) const {
    std::map<int, Slave>::const_iterator it = mSlaves.find(slaveId);
    return it->second.getReadCount();
}

int
MockedModbusContext::getWriteCount(int slaveId) const {
    std::map<int, Slave>::const_iterator it = mSlaves.find(slaveId);
    return it->second.getWriteCount();
}

std::shared_ptr<MockedModbusContext>
MockedModbusFactory::getOrCreateContext(const char* network) {
    auto it = mModbusNetworks.find(network);
    std::shared_ptr<MockedModbusContext> ctx;
    if (it == mModbusNetworks.end()) {
        ctx.reset(new MockedModbusContext());
        modmqttd::ModbusNetworkConfig c;
        c.mName = network;
        ctx->init(c);
        mModbusNetworks[network] = ctx;
    } else {
        ctx = it->second;
    }
    return ctx;
}

void
MockedModbusFactory::setModbusRegisterValue(const char* network, int slaveId, int regNum, modmqttd::RegisterType regtype, uint16_t val) {
    std::shared_ptr<MockedModbusContext> ctx = getOrCreateContext(network);
    modmqttd::MsgRegisterValues msg(slaveId, regtype, regNum, val);
    ctx->mInternalOperation = true;
    ctx->writeModbusRegisters(msg);
}

void
MockedModbusFactory::setModbusRegisterReadError(const char* network, int slaveId, int regNum, modmqttd::RegisterType regType) {
    std::shared_ptr<MockedModbusContext> ctx = getOrCreateContext(network);
    MockedModbusContext::Slave& s(ctx->getSlave(slaveId));
    s.setError(regNum, regType);
}


void
MockedModbusFactory::disconnectModbusSlave(const char* network, int slaveId) {
    std::shared_ptr<MockedModbusContext> ctx = getOrCreateContext(network);
    ctx->getSlave(slaveId).setDisconnected();
}


