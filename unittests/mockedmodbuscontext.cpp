#include "mockedmodbuscontext.hpp"

#include <thread>
#include <iostream>

#include "catch2/catch_all.hpp"

#include "libmodmqttsrv/modbus_messages.hpp"
#include "libmodmqttsrv/modbus_context.hpp"
#include "libmodmqttsrv/debugtools.hpp"
#include "libmodmqttsrv/register_poll.hpp"

const std::chrono::milliseconds MockedModbusContext::sDefaultSlaveReadTime = std::chrono::milliseconds(5);
const std::chrono::milliseconds MockedModbusContext::sDefaultSlaveWriteTime = std::chrono::milliseconds(10);

void
MockedModbusContext::Slave::write(const modmqttd::RegisterWrite& msg, bool internalOperation) {
    if (!internalOperation) {
        std::this_thread::sleep_for(mWriteTime);
        mWriteCount++;
        if (mDisconnected) {
            errno = EIO;
            throw modmqttd::ModbusWriteException(std::string("write fn ") + std::to_string(msg.mRegister) + " failed");
        }
        if (hasError(msg.mRegister, msg.mRegisterType, msg.getCount())) {
            errno = EIO;
            throw modmqttd::ModbusWriteException(std::string("register write fn ") + std::to_string(msg.mRegister) + " failed");
        }
    }


    for(int i = 0; i < msg.getCount(); i++) {
        int regNumber = msg.mRegister + i;
        uint16_t value = msg.mValues.getValue(i);
        switch(msg.mRegisterType) {
            case modmqttd::RegisterType::COIL:
                mCoil[regNumber].mValue = value == 1;
            break;
            case modmqttd::RegisterType::BIT:
                mBit[regNumber].mValue = value == 1;
            break;
            case modmqttd::RegisterType::HOLDING:
                mHolding[regNumber].mValue = value;
            break;
            case modmqttd::RegisterType::INPUT:
                mInput[regNumber].mValue = value;
            break;
            default:
                throw modmqttd::ModbusWriteException(std::string("Cannot write, unknown register type ") + std::to_string(msg.mRegisterType));
        };
    }

    if (!internalOperation) {
        mIOCondition->notify_all();
    }
}

std::vector<uint16_t>
MockedModbusContext::Slave::read(const modmqttd::RegisterPoll& regData, bool internalOperation) {
    if (!internalOperation) {
        std::this_thread::sleep_for(mReadTime);
        mReadCount++;
        if (mDisconnected) {
            errno = EIO;
            throw modmqttd::ModbusReadException(std::string("read fn ") + std::to_string(regData.mRegister) + " failed");
        }
        if (hasError(regData.mRegister, regData.mRegisterType, regData.getCount())) {
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
MockedModbusContext::Slave::hasError(int regNum, modmqttd::RegisterType regType, int regCount) const {
    switch(regType) {
        case modmqttd::RegisterType::COIL:
            return hasError(mCoil, regNum, regCount);
        break;
        case modmqttd::RegisterType::HOLDING:
            return hasError(mHolding, regNum, regCount);
        break;
        case modmqttd::RegisterType::INPUT:
            return hasError(mInput, regNum, regCount);
        break;
        case modmqttd::RegisterType::BIT:
            return hasError(mBit, regNum, regCount);
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
    std::vector<uint16_t> ret;
    ret.reserve(count);
    for (int i = num; i < num + count; i++) {
        ret.push_back(readRegister(table, i));
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
MockedModbusContext::Slave::hasError(const std::map<int, MockedModbusContext::Slave::RegData>& table, int num, int count) const {
    bool ret = false;
    for (int i = num; i < num + count; i++) {
        auto it = table.find(i);
        if (it == table.end())
            continue;
        if (it->second.mError)
            return true;
    }
    return ret;
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
        BOOST_LOG_SEV(log, modmqttd::Log::info) << "TEST: modbus " << mNetworkName
            << "." << it->second.mId << "." << regData.mRegister
            << " READ: " << modmqttd::DebugTools::registersToStr(ret);

    mInternalOperation = false;
    mLastPollTime = std::chrono::steady_clock::now();
    mLastPolledSlave = slaveId;
    mLastPolledRegister = regData.mRegister;
    return ret;
}

void
MockedModbusContext::init(const modmqttd::ModbusNetworkConfig& config) {
    mNetworkName = config.mName;
    std::string fname = std::string("_") + mNetworkName;
    if (config.mType == modmqttd::ModbusNetworkConfig::Type::RTU)
        fname = config.mDevice;

    mDeviceName = fname;
    createFakeDevice();
}

void
MockedModbusContext::createFakeDevice() {
    std::ofstream s(mDeviceName);
    s.close();
}


void
MockedModbusContext::connect() {
    mConnectionCount++;
    mDeviceFile.open(mDeviceName);
}

bool
MockedModbusContext::isConnected() const {
    return mDeviceFile.is_open();
}

void
MockedModbusContext::disconnect() {
    mDeviceFile.close();
}


void
MockedModbusContext::writeModbusRegisters(int pSlaveId, const modmqttd::RegisterWrite& msg) {
    std::unique_lock<std::mutex> lck(mMutex);
    std::map<int, Slave>::iterator it = findOrCreateSlave(pSlaveId);

    if (mInternalOperation)
        BOOST_LOG_SEV(log, modmqttd::Log::info) << "TEST: modbus: " << mNetworkName
            << "." << it->second.mId << "." << msg.mRegister
            << " WRITE: " << modmqttd::DebugTools::registersToStr(msg.mValues.values());
    it->second.write(msg, mInternalOperation);
    mInternalOperation = false;
}

std::map<int, MockedModbusContext::Slave>::iterator
MockedModbusContext::findOrCreateSlave(int id) {
    std::map<int, Slave>::iterator it = mSlaves.find(id);
    if (it == mSlaves.end()) {
        Slave s(mCondition, id);
        mSlaves.insert(std::make_pair(id, s));
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

uint16_t
MockedModbusContext::getModbusRegisterValue(int slaveId, int regNum, modmqttd::RegisterType regtype) {
    mInternalOperation = true;
    modmqttd::RegisterPoll poll(slaveId, --regNum, regtype, 1, std::chrono::milliseconds(0), modmqttd::PublishMode::ON_CHANGE);

    auto vals = readModbusRegisters(slaveId, poll);
    return vals[0];
}


uint16_t
MockedModbusContext::waitForModbusValue(int slaveId, int regNum, modmqttd::RegisterType regType, uint16_t val, std::chrono::milliseconds timeout) {
    BOOST_LOG_SEV(log, modmqttd::Log::info) << "TEST: Waiting " << timeout.count() << "ms for value " << val << " in register " << slaveId << "." << regNum << ", type=" << std::to_string(regType);

    std::mutex m;
    std::unique_lock<std::mutex> lck(m);

    uint16_t currentVal = getModbusRegisterValue(slaveId, regNum, regType);
    if (currentVal != val) {
        auto start = std::chrono::steady_clock::now();
        int dur;
        do {
            if (mCondition->wait_for(lck, timeout) == std::cv_status::timeout)
                break;
            currentVal = getModbusRegisterValue(slaveId, regNum, regType);

            if (currentVal == val)
                break;
            auto end = std::chrono::steady_clock::now();
            dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        } while (dur < timeout.count());
    }
    return currentVal;
}



std::shared_ptr<MockedModbusContext>
MockedModbusFactory::getOrCreateContext(const char* network) {
    auto it = mModbusNetworks.find(network);
    std::shared_ptr<MockedModbusContext> ctx;
    if (it == mModbusNetworks.end()) {
        ctx.reset(new MockedModbusContext());
        ctx->mNetworkName = network;
        mModbusNetworks[network] = ctx;
    } else {
        ctx = it->second;
    }
    return ctx;
}

std::shared_ptr<MockedModbusContext>
MockedModbusFactory::findOrReturnFirstContext(const char* network) const {
    auto it = mModbusNetworks.begin();
    if (network != nullptr)
        it = mModbusNetworks.find(network);
    return it->second;
}

std::chrono::time_point<std::chrono::steady_clock>
MockedModbusFactory::getLastPollTime(const char* network) const {
    auto ctx = findOrReturnFirstContext(network);
    return ctx->getLastPollTime();
}


void
MockedModbusFactory::setModbusRegisterValue(const char* network, int slaveId, int regNum, modmqttd::RegisterType regtype, uint16_t val) {
    regNum--;
    std::shared_ptr<MockedModbusContext> ctx = getOrCreateContext(network);
    modmqttd::RegisterWrite msg(slaveId, regNum, regtype, ModbusRegisters(val));
    ctx->mInternalOperation = true;
    ctx->writeModbusRegisters(msg.mSlaveId, msg);
}


uint16_t
MockedModbusFactory::getModbusRegisterValue(const char* network, int slaveId, int regNum, modmqttd::RegisterType regtype) {
    std::shared_ptr<MockedModbusContext> ctx = getOrCreateContext(network);
    return ctx->getModbusRegisterValue(slaveId, regNum, regtype);
}


void
MockedModbusFactory::setModbusRegisterReadError(const char* network, int slaveId, int regNum, modmqttd::RegisterType regType) {
    regNum--;
    std::shared_ptr<MockedModbusContext> ctx = getOrCreateContext(network);
    MockedModbusContext::Slave& s(ctx->getSlave(slaveId));
    s.setError(regNum, regType);
}

void
MockedModbusFactory::setModbusRegisterWriteError(const char* network, int slaveId, int regNum, modmqttd::RegisterType regType) {
    regNum--;
    std::shared_ptr<MockedModbusContext> ctx = getOrCreateContext(network);
    MockedModbusContext::Slave& s(ctx->getSlave(slaveId));
    s.setError(regNum, regType);
}

void
MockedModbusFactory::disconnectModbusSlave(const char* network, int slaveId) {
    std::shared_ptr<MockedModbusContext> ctx = getOrCreateContext(network);
    ctx->getSlave(slaveId).setDisconnected();
}

void
MockedModbusFactory::connectModbusSlave(const char* network, int slaveId) {
    std::shared_ptr<MockedModbusContext> ctx = getOrCreateContext(network);
    ctx->getSlave(slaveId).setDisconnected(false);
}


std::tuple<int, int>
MockedModbusFactory::getLastReadRegisterAddress(const char* network) const {
    std::shared_ptr<MockedModbusContext> ctx = findOrReturnFirstContext(network);
    return ctx->getLastReadRegisterAddress();
}


uint16_t
MockedModbusFactory::waitForModbusValue(const char* network, int slaveId, int regNum, modmqttd::RegisterType regType, uint16_t val, std::chrono::milliseconds timeout) {
    std::shared_ptr<MockedModbusContext> ctx = findOrReturnFirstContext(network);
    return ctx->waitForModbusValue(slaveId, regNum, regType, val, timeout);
}
