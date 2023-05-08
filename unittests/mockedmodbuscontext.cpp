#include "mockedmodbuscontext.hpp"

#include "libmodmqttsrv/modbus_messages.hpp"
#include "libmodmqttsrv/modbus_context.hpp"
#include "catch2/catch.hpp"

#include <thread>
#include <iostream>

const std::chrono::milliseconds MockedModbusContext::sDefaultSlaveReadTime = std::chrono::milliseconds(5);
const std::chrono::milliseconds MockedModbusContext::sDefaultSlaveWriteTime = std::chrono::milliseconds(10);

void
MockedModbusContext::Slave::write(int registerAddress, modmqttd::RegisterType registerType, uint16_t value, bool internalOperation) {
    if (!internalOperation) {
        std::this_thread::sleep_for(mWriteTime);
        if (mDisconnected) {
            errno = EIO;
            throw modmqttd::ModbusWriteException(std::string("write fn ") + std::to_string(registerAddress) + " failed");
        }
        if (hasError(registerAddress, registerType)) {
            errno = EIO;
            throw modmqttd::ModbusReadException(std::string("register write fn ") + std::to_string(registerAddress) + " failed");
        }
    }
    switch(registerType) {
        case modmqttd::RegisterType::COIL:
            mCoil[registerAddress].mValue = value == 1;
            mCoil[registerAddress].mReadCount = 0;
        break;
        case modmqttd::RegisterType::BIT:
            mBit[registerAddress].mValue = value == 1;
            mBit[registerAddress].mReadCount = 0;
        break;
        case modmqttd::RegisterType::HOLDING:
            mHolding[registerAddress].mValue = value;
            mHolding[registerAddress].mReadCount = 0;
        break;
        case modmqttd::RegisterType::INPUT:
            mInput[registerAddress].mValue = value;
            mInput[registerAddress].mReadCount = 0;
        break;
        default:
            throw modmqttd::ModbusWriteException(std::string("Cannot write, unknown register type ") + std::to_string(registerType));
    };
}

uint16_t
MockedModbusContext::Slave::read(int registerAddress, modmqttd::RegisterType registerType, bool internalOperation) {
    if (!internalOperation) {
        std::this_thread::sleep_for(mReadTime);
        if (mDisconnected) {
            errno = EIO;
            throw modmqttd::ModbusReadException(std::string("read fn ") + std::to_string(registerAddress) + " failed");
        }
        if (hasError(registerAddress, registerType)) {
            errno = EIO;
            throw modmqttd::ModbusReadException(std::string("register read fn ") + std::to_string(registerAddress) + " failed");
        }
    }
    switch(registerType) {
        case modmqttd::RegisterType::COIL:
            return readRegister(mCoil, registerAddress, internalOperation);
        break;
        case modmqttd::RegisterType::HOLDING:
            return readRegister(mHolding, registerAddress, internalOperation);
        break;
        case modmqttd::RegisterType::INPUT:
            return readRegister(mInput, registerAddress, internalOperation);
        break;
        case modmqttd::RegisterType::BIT:
            return readRegister(mBit, registerAddress, internalOperation);
        break;
        default:
            throw modmqttd::ModbusReadException(std::string("Cannot read, unknown register type ") + std::to_string(registerType));
    };
}

int
MockedModbusContext::Slave::getReadCount(int regNum, modmqttd::RegisterType regType) {
    switch(regType) {
        case modmqttd::RegisterType::COIL:
            return mCoil[regNum].mReadCount;
        case modmqttd::RegisterType::BIT:
            return mBit[regNum].mReadCount;
        case modmqttd::RegisterType::HOLDING:
            return mHolding[regNum].mReadCount;
        case modmqttd::RegisterType::INPUT:
            return mInput[regNum].mReadCount;
        default:
            throw modmqttd::ModbusReadException(std::string("Cannot set error, unknown register type ") + std::to_string(regType));
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

uint16_t
MockedModbusContext::Slave::readRegister(std::map<int, MockedModbusContext::Slave::RegData>& table, int num, bool internalOperation) {
    auto it = table.find(num);
    if (it == table.end())
        return 0;
    if (!internalOperation) {
        it->second.mReadCount++;
    }
    return it->second.mValue;
}

bool
MockedModbusContext::Slave::hasError(const std::map<int, MockedModbusContext::Slave::RegData>& table, int num) const {
    auto it = table.find(num);
    if (it == table.end())
        return false;
    return it->second.mError;
}

uint16_t
MockedModbusContext::readModbusRegister(int slaveId, const modmqttd::RegisterPoll& regData) {
    std::unique_lock<std::mutex> lck(mMutex);
    std::map<int, Slave>::iterator it = findOrCreateSlave(slaveId);
    uint16_t ret = it->second.read(regData.mRegisterAddress, regData.mRegisterType, mInternalOperation);
    if (mInternalOperation)
        BOOST_LOG_SEV(log, modmqttd::Log::info) << "MODBUS: " << mNetworkName
            << "." << it->second.mId << "." << regData.mRegisterAddress
            << " READ: " << ret;

    mInternalOperation = false;
    return ret;
}

static std::string toStr(const std::vector<uint16_t>& vec) {
    std::stringstream str;
    for (auto it = vec.begin(); it != vec.end(); ++it) {
        str << (*it) << ", ";
    }
    return str.str();
}

std::vector<uint16_t>
MockedModbusContext::readModbusRegisters(const modmqttd::MsgRegisterReadRemoteCall& msg) {
    std::vector<uint16_t> ret;
    std::unique_lock<std::mutex> lck(mMutex);
    std::map<int, Slave>::iterator it = findOrCreateSlave(msg.mSlaveId);
    for (int i = 0; i < msg.mSize; i++) {
        ret.push_back(it->second.read(msg.mRegisterAddress + i, msg.mRegisterType, mInternalOperation));
    }
    if (mInternalOperation)
        BOOST_LOG_SEV(log, modmqttd::Log::info) << "MODBUS: " << mNetworkName
            << "." << it->second.mId << "." << msg.mRegisterAddress << "[" << msg.mSize << "]"
            << " READ: " << toStr(ret);

    mInternalOperation = false;
    return ret;
}

int MockedModbusContext::getRegisterReadCount(int slaveId, int registerAddress, modmqttd::RegisterType registerType) {
    std::unique_lock<std::mutex> lck(mMutex);
    std::map<int, Slave>::iterator it = findOrCreateSlave(slaveId);
    return it->second.getReadCount(registerAddress, registerType);
}

void
MockedModbusContext::init(const modmqttd::ModbusNetworkConfig& config) {
    mNetworkName = config.mName;
}

void
MockedModbusContext::writeModbusRegister(const modmqttd::MsgRegisterValue& msg) {
    std::unique_lock<std::mutex> lck(mMutex);
    std::map<int, Slave>::iterator it = findOrCreateSlave(msg.mSlaveId);

    if (mInternalOperation) {
        BOOST_LOG_SEV(log, modmqttd::Log::info) << "MODBUS: " << mNetworkName
            << "." << it->second.mId << "." << msg.mRegisterAddress
            << " WRITE: " << msg.mValue;
    }
    it->second.write(msg.mRegisterAddress, msg.mRegisterType, msg.mValue, mInternalOperation);
    mInternalOperation = false;
}

void
MockedModbusContext::writeModbusRegisters(const modmqttd::MsgRegisterWriteRemoteCall& msg) {
    std::unique_lock<std::mutex> lck(mMutex);
    std::map<int, Slave>::iterator it = findOrCreateSlave(msg.mSlaveId);

    if (mInternalOperation) {
        BOOST_LOG_SEV(log, modmqttd::Log::info) << "MODBUS: " << mNetworkName
            << "." << it->second.mId << "." << msg.mRegisterAddress
            << " WRITE: [" << toStr(msg.mValues) << "]";
    }

    // error check
    if (!mInternalOperation) {
        int address = msg.mRegisterAddress;
        for (auto itVal = msg.mValues.begin(); itVal != msg.mValues.end(); ++itVal, ++address) {
            if (it->second.hasError(address, msg.mRegisterType)) {
                errno = EIO;
                throw modmqttd::ModbusReadException(std::string("register write fn ") + std::to_string(address) + " failed");
            }
        }
    }

    int address = msg.mRegisterAddress;
    for (auto itVal = msg.mValues.begin(); itVal != msg.mValues.end(); ++itVal, ++address) {
        it->second.write(address, msg.mRegisterType, *itVal, mInternalOperation);
    }
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

uint16_t
MockedModbusFactory::getModbusRegisterValue(const char* network, int slaveId, int regNum, modmqttd::RegisterType regtype) {
    std::shared_ptr<MockedModbusContext> ctx = getOrCreateContext(network);
    modmqttd::RegisterPoll msg(regNum, regtype, 0);
    ctx->mInternalOperation = true;
    return ctx->readModbusRegister(slaveId, msg);
}

void
MockedModbusFactory::setModbusRegisterValue(const char* network, int slaveId, int regNum, modmqttd::RegisterType regtype, uint16_t val) {
    std::shared_ptr<MockedModbusContext> ctx = getOrCreateContext(network);
    modmqttd::MsgRegisterValue msg(slaveId, regtype, regNum, val);
    ctx->mInternalOperation = true;
    ctx->writeModbusRegister(msg);
}

void
MockedModbusFactory::setModbusRegisterError(const char* network, int slaveId, int regNum, modmqttd::RegisterType regType) {
    std::shared_ptr<MockedModbusContext> ctx = getOrCreateContext(network);
    MockedModbusContext::Slave& s(ctx->getSlave(slaveId));
    s.setError(regNum, regType);
}

int
MockedModbusFactory::getRegisterReadCount(const char* network, int slaveId, int regNum, modmqttd::RegisterType registerType) {
    std::shared_ptr<MockedModbusContext> ctx = getOrCreateContext(network);
    return ctx->getRegisterReadCount(slaveId, regNum, registerType);
}

void
MockedModbusFactory::disconnectModbusSlave(const char* network, int slaveId) {
    std::shared_ptr<MockedModbusContext> ctx = getOrCreateContext(network);
    ctx->getSlave(slaveId).setDisconnected();
}


