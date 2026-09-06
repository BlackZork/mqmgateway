#pragma once

#include <string>
#include <map>
#include <chrono>
#include <vector>

#include "libmodmqttconv/modbusregisters.hpp"

#include "common.hpp"
#include "modbus_types.hpp"
#include "debugtools.hpp"

namespace modmqttd {

class MsgRegisterValues : public ModbusMessageBase {
    public:
        MsgRegisterValues(int slaveId, RegisterType regType, int registerNumber, const ModbusRegisters& registers, int pCommandId, ModbusWriteMode pWriteMode)
            : ModbusMessageBase(slaveId, registerNumber, regType, registers.getCount(), pCommandId),
              mRegisters(registers),
              mWriteMode(pWriteMode),
              mCreationTime(std::chrono::steady_clock::now()) {}
        MsgRegisterValues(int pSlaveId, RegisterType pRegType, int pRegisterNumber, const std::vector<uint16_t>& pRegisters, int pCommandId,
                          const std::chrono::steady_clock::time_point& pReadStartTime)
            : ModbusMessageBase(pSlaveId, pRegisterNumber, pRegType, pRegisters.size(), pCommandId),
              mRegisters(pRegisters),
              mCreationTime(std::chrono::steady_clock::now()),
              mReadStartTime(pReadStartTime) {}

        const std::chrono::steady_clock::time_point& getCreationTime() const { return mCreationTime; }

        /**
         * When the modbus read that produced these values started, or
         * time_point::min() if they did not come from a read - a write
         * confirmation carries the values back without reading them.
         *
         * ModbusScheduler times the next poll from the same value, so a
         * republish decision made against it follows the poll cadence instead
         * of wall clock on the main thread.
         */
        const std::chrono::steady_clock::time_point& getReadStartTime() const { return mReadStartTime; }

        ModbusRegisters mRegisters;
        ModbusWriteMode mWriteMode = ModbusWriteMode::AUTO;

    private:
        std::chrono::steady_clock::time_point mCreationTime;
        std::chrono::steady_clock::time_point mReadStartTime = std::chrono::steady_clock::time_point::min();
};

class MsgRegisterReadFailed : public ModbusMessageBase {
    public:
        MsgRegisterReadFailed(int pSlaveId, RegisterType pRegType, int pRegisterNumber, int pRegisterCount, int pCommandId = 0)
            : ModbusMessageBase(pSlaveId, pRegisterNumber, pRegType, pRegisterCount, pCommandId) {}
};

class MsgRegisterWriteFailed : public ModbusMessageBase {
    public:
        MsgRegisterWriteFailed(int pSlaveId, RegisterType pRegType, int pRegisterNumber, int pRegisterCount, int pCommandId = 0)
            : ModbusMessageBase(pSlaveId, pRegisterNumber, pRegType, pRegisterCount, pCommandId) {}
};

class MsgRegisterReadRequest : public ModbusMessageBase {
    public:
        MsgRegisterReadRequest(int pSlaveId, RegisterType pRegType, int pRegisterNumber, int pRegisterCount, int pCommandId)
            : ModbusMessageBase(pSlaveId, pRegisterNumber, pRegType, pRegisterCount, pCommandId) {}
};


class MsgRegisterPoll : public ModbusMessageBase {
    public:
        static constexpr std::chrono::milliseconds INVALID_REFRESH = std::chrono::milliseconds(-1);
        MsgRegisterPoll(int pSlaveId, int pRegisterNumber, RegisterType pType, int pCount = 1)
            : ModbusMessageBase(pSlaveId, pRegisterNumber, pType, pCount) {
#ifndef NDEBUG
            if (mRegister < 0)
                throw DebugException("Invalid register number");
            if (mCount <= 0)
                throw DebugException("Count cannot be 0 or negative");
#endif
        }

        bool isSameAs(const MsgRegisterPoll& other) const;
        void merge(const MsgRegisterPoll& other);

        std::chrono::milliseconds mRefreshMsec = INVALID_REFRESH;
        PublishMode mPublishMode = PublishMode::ON_CHANGE;
};

class MsgRegisterPollSpecification {
    public:
        MsgRegisterPollSpecification(const std::string& networkName) : mNetworkName(networkName) {}

        /*!
            Convert subsequent slave registers
            of the same type to single MsgRegisterPoll instance
            with corresponding mCount value

            This method will not join overlapping groups.
        */
        void group();

        void merge(const std::vector<MsgRegisterPoll>& lst) {
            for (auto& poll: lst) {
                {
                    merge(poll);
                }
            }
        }

        /*!
            Merge overlapping
            register group with arg and adjust refresh time
            or add new register to poll

        */
        void merge(const MsgRegisterPoll& poll);

        std::string mNetworkName;
        std::vector<MsgRegisterPoll> mRegisters;
};

class MsgModbusNetworkState {
    public:
        MsgModbusNetworkState(const std::string& networkName, bool isUp)
            : mIsUp(isUp), mNetworkName(networkName) {}
        bool mIsUp;
        std::string mNetworkName;
};

class MsgMqttNetworkState {
    public:
        MsgMqttNetworkState(bool isUp)
            : mIsUp(isUp) {}
        bool mIsUp;
};

class EndWorkMessage {
        // no fields here, thread will check type of message and exit
};

}
