#pragma once

#include <chrono>
#include <vector>

#include "libmodmqttconv/modbusregisters.hpp"


#include "modbus_types.hpp"
#include "modbus_slave.hpp"

namespace modmqttd {

class IRegisterCommand {
    public:
        virtual int getRegister() const = 0;
        virtual const ModbusCommandDelay& getDelay() const = 0;
        const bool hasDelay() const {
            return getDelay() != std::chrono::steady_clock::duration::zero();
        }
        virtual int getCount() const = 0;
        virtual const std::vector<uint16_t>& getValues() const = 0;
};


class RegisterPoll : public IRegisterCommand {
    public:
        static constexpr std::chrono::steady_clock::duration DurationBetweenLogError = std::chrono::minutes(5);
        // if we cannot read register in this time MsgRegisterReadFailed is sent
        static constexpr int DefaultReadErrorCount = 3;

        RegisterPoll(int regNum, RegisterType regType, int regCount, std::chrono::milliseconds refreshMsec);

        virtual int getRegister() const { return mRegister; };
        virtual int getCount() const { return mLastValues.size(); }
        virtual const std::vector<uint16_t>& getValues() const { return mLastValues; }
        virtual const ModbusCommandDelay& getDelay() const { return mDelay; }

        void update(const std::vector<uint16_t> newValues) { mLastValues = newValues; }
        void updateFromSlaveConfig(const ModbusSlaveConfig& slave_config);

        int mRegister;
        RegisterType mRegisterType;
        std::chrono::steady_clock::duration mRefresh;

        // delay before poll
        ModbusCommandDelay mDelay;

        std::chrono::steady_clock::time_point mLastRead;

        int mReadErrors;
        std::chrono::steady_clock::time_point mFirstErrorTime;
    private:
        std::vector<uint16_t> mLastValues;
};

class RegisterWrite : public IRegisterCommand {
    public:
        RegisterWrite(int pRegister, RegisterType pType, ModbusRegisters& pValues)
            : mRegister(pRegister),
              mRegisterType(pType),
              mValues(pValues)
        {}
        virtual int getRegister() const { return mRegister; };
        virtual const ModbusCommandDelay& getDelay() const { return mDelay; }
        virtual int getCount() { return mValues.getCount(); };
        virtual const std::vector<uint16_t>& getValues() const { return mValues.values(); }

        ModbusCommandDelay mDelay;
    private:
        int mRegister;
        RegisterType mRegisterType;
        ModbusRegisters mValues;
};

} //namespace
