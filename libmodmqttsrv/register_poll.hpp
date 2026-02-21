#pragma once

#include <chrono>
#include <vector>

#include "libmodmqttconv/modbusregisters.hpp"

#include "common.hpp"
#include "modbus_messages.hpp"
#include "modbus_types.hpp"

namespace modmqttd {

class RegisterCommand : public ModbusAddressRange {
    public:
        RegisterCommand(int pSlaveId, int pRegister, RegisterType pRegisterType, int pCount)
            : ModbusAddressRange(pRegister, pRegisterType, pCount),
              mSlaveId(pSlaveId)
        {}

        virtual int getRegister() const = 0;
        const bool hasDelay() const {
            return mDelayBeforeCommand != std::chrono::steady_clock::duration::zero()
                || mDelayBeforeFirstCommand != std::chrono::steady_clock::duration::zero();
        }


        virtual int getCount() const = 0;
        virtual const std::vector<uint16_t>& getValues() const = 0;

        virtual bool executedOk() const = 0;

        bool hasDelayBeforeFirstCommand() const { return mDelayBeforeFirstCommand != std::chrono::steady_clock::duration::zero(); }
        bool hasDelayBeforeCommand() const { return mDelayBeforeCommand != std::chrono::steady_clock::duration::zero(); }

        const std::chrono::steady_clock::duration& getDelayBeforeFirstCommand() const { return mDelayBeforeFirstCommand; }
        const std::chrono::steady_clock::duration& getDelayBeforeCommand() const { return mDelayBeforeCommand; }

        void setDelayBeforeFirstCommand(const std::chrono::steady_clock::duration& pDelay) { mDelayBeforeFirstCommand = pDelay; }
        void setDelayBeforeCommand(const std::chrono::steady_clock::duration& pDelay) { mDelayBeforeCommand = pDelay; }

        void setMaxRetryCounts(short pMaxRead, short pMaxWrite, bool pForce = false);

        int mSlaveId;

        short mMaxReadRetryCount;
        short mMaxWriteRetryCount;
    protected:
        std::chrono::steady_clock::duration mDelayBeforeFirstCommand = std::chrono::steady_clock::duration::zero();
        std::chrono::steady_clock::duration mDelayBeforeCommand = std::chrono::steady_clock::duration::zero();
};


class RegisterPoll : public RegisterCommand {
    public:
        static constexpr std::chrono::steady_clock::duration DurationBetweenLogError = std::chrono::minutes(5);
        // if we cannot read register in this time MsgRegisterReadFailed is sent
        static constexpr int DefaultReadErrorCount = 3;

        RegisterPoll(int pSlaveId, int pRegNum, RegisterType pRegType, int pRegCount, std::chrono::milliseconds pRrefreshMsec, PublishMode pPublishMode);

        virtual int getRegister() const { return mRegister; };
        virtual int getCount() const { return mLastValues.size(); }
        virtual const std::vector<uint16_t>& getValues() const { return mLastValues; }
        virtual bool executedOk() const { return mLastReadOk; };


        void update(const std::vector<uint16_t>& newValues) {
            mLastValues = newValues;
            mCount = newValues.size();
        }

        std::chrono::steady_clock::duration mRefresh;

        bool mLastReadOk = false;
        std::chrono::steady_clock::time_point mLastRead;

        int mReadErrors;
        std::chrono::steady_clock::time_point mFirstErrorTime;

        PublishMode mPublishMode = PublishMode::ON_CHANGE;
    private:
        std::vector<uint16_t> mLastValues;
};

class RegisterWrite : public RegisterCommand {
    public:
        RegisterWrite(const MsgRegisterValues& msg)
            : RegisterCommand(msg.mSlaveId, msg.mRegister, msg.mRegisterType, msg.mRegisters.getCount()),
              mCreationTime(msg.getCreationTime()),
              mValues(msg.mRegisters)
        {}
        RegisterWrite(int pSlaveId, int pRegister, RegisterType pType, const ModbusRegisters& pValues)
            : RegisterCommand(pSlaveId, pRegister, pType, pValues.getCount()),
              mCreationTime(std::chrono::steady_clock::now()),
              mValues(pValues)
        {}

        virtual int getRegister() const { return mRegister; };
        virtual int getCount() const { return mValues.getCount(); };
        virtual const std::vector<uint16_t>& getValues() const { return mValues.values(); }
        virtual bool executedOk() const { return mLastWriteOk; };

        ModbusRegisters mValues;

        bool mLastWriteOk = false;
        std::chrono::steady_clock::time_point mCreationTime;

        std::shared_ptr<MsgRegisterValues> mReturnMessage;
};

} //namespace
