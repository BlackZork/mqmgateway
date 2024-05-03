#pragma once

#include <chrono>
#include <vector>

#include "libmodmqttconv/modbusregisters.hpp"

#include "modbus_messages.hpp"
#include "modbus_types.hpp"

namespace modmqttd {

class IRegisterCommand {
    public:
        virtual int getRegister() const = 0;
        virtual const ModbusCommandDelay& getDelay() const = 0;
        const bool hasDelay() const {
            return getDelay() != std::chrono::steady_clock::duration::zero();
        }
        virtual void setDelay(const ModbusCommandDelay& pDelay) = 0;
        virtual int getCount() const = 0;
        virtual const std::vector<uint16_t>& getValues() const = 0;
};


class RegisterPoll : public ModbusAddressRange, public IRegisterCommand {
    public:
        static constexpr std::chrono::steady_clock::duration DurationBetweenLogError = std::chrono::minutes(5);
        // if we cannot read register in this time MsgRegisterReadFailed is sent
        static constexpr int DefaultReadErrorCount = 3;

        RegisterPoll(int regNum, RegisterType regType, int regCount, std::chrono::milliseconds refreshMsec);

        virtual int getRegister() const { return mRegister; };
        virtual int getCount() const { return mLastValues.size(); }
        virtual const std::vector<uint16_t>& getValues() const { return mLastValues; }
        virtual const ModbusCommandDelay& getDelay() const { return mDelay; }
        virtual void setDelay(const ModbusCommandDelay& pDelay) { mDelay = pDelay; }

        void update(const std::vector<uint16_t> newValues) { mLastValues = newValues; mCount = newValues.size(); }

        std::chrono::steady_clock::duration mRefresh;


        std::chrono::steady_clock::time_point mLastRead;

        int mReadErrors;
        std::chrono::steady_clock::time_point mFirstErrorTime;
    private:
        std::vector<uint16_t> mLastValues;

        // delay before poll
        ModbusCommandDelay mDelay;
};

class RegisterWrite : public ModbusAddressRange, public IRegisterCommand {
    public:
        RegisterWrite(const MsgRegisterValues& msg)
            : RegisterWrite(msg.mRegister,
              msg.mRegisterType,
              msg.mRegisters
              )
        {}
        RegisterWrite(int pRegister, RegisterType pType, const ModbusRegisters& pValues)
            : ModbusAddressRange(pRegister, pType, pValues.getCount()),
              mValues(pValues),
              mDelay(std::chrono::milliseconds::zero())
        {}

        virtual int getRegister() const { return mRegister; };
        virtual const ModbusCommandDelay& getDelay() const { return mDelay; }
        virtual int getCount() const { return mValues.getCount(); };
        virtual const std::vector<uint16_t>& getValues() const { return mValues.values(); }
        virtual void setDelay(const ModbusCommandDelay& pDelay) { mDelay = pDelay; }

        ModbusRegisters mValues;

        std::shared_ptr<MsgRegisterValues> mReturnMessage;
    private:
        ModbusCommandDelay mDelay;
};

} //namespace
