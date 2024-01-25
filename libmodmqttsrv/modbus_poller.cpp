#include "modbus_poller.hpp"
#include "modbus_messages.hpp"
#include "modbus_thread.hpp"
#include "queue_item.hpp"

namespace modmqttd {

ModbusPoller::ModbusPoller(moodycamel::BlockingReaderWriterQueue<QueueItem>& fromModbusQueue)
    : mFromModbusQueue(fromModbusQueue)
{}

void
ModbusPoller::sendMessage(const QueueItem& item) {
    ModbusThread::sendMessageFromModbus(mFromModbusQueue, item);
}

void
ModbusPoller::pollRegisters(int slaveId, const std::vector<std::shared_ptr<RegisterPoll>>& registers, bool sendIfChanged) {
    for(std::vector<std::shared_ptr<RegisterPoll>>::const_iterator reg_it = registers.begin();
        reg_it != registers.end(); reg_it++)
    {
        try {
            std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
            RegisterPoll& reg(**reg_it);
            std::vector<uint16_t> newValues(mModbus->readModbusRegisters(slaveId, reg));
            reg.mLastRead = std::chrono::steady_clock::now();

            std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
            BOOST_LOG_SEV(log, Log::debug) << "Register " << slaveId << "." << reg.mRegister << " (0x" << std::hex << slaveId << ".0x" << std::hex << reg.mRegister << ")"
                            << " polled in "  << std::dec << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms";

            if ((reg.getValues() != newValues) || !sendIfChanged || (reg.mReadErrors != 0)) {
                MsgRegisterValues val(slaveId, reg.mRegisterType, reg.mRegister, newValues);
                sendMessage(QueueItem::create(val));
                reg.update(newValues);
                reg.mReadErrors = 0;
                BOOST_LOG_SEV(log, Log::debug) << "Register " << slaveId << "." << reg.mRegister
                    << " values sent, data=" << DebugTools::registersToStr(reg.getValues());
            };
        } catch (const ModbusReadException& ex) {
            handleRegisterReadError(slaveId, **reg_it, ex.what());
        };
    };
};

void
ModbusPoller::handleRegisterReadError(int slaveId, RegisterPoll& regPoll, const char* errorMessage) {
    // avoid flooding logs with register read error messages - log last error every 5 minutes
    regPoll.mReadErrors++;
    if (regPoll.mReadErrors == 1 || (std::chrono::steady_clock::now() - regPoll.mFirstErrorTime > RegisterPoll::DurationBetweenLogError)) {
        BOOST_LOG_SEV(log, Log::error) << regPoll.mReadErrors << " error(s) when reading register "
            << slaveId << "." << regPoll.mRegister << ", last error: " << errorMessage;
        regPoll.mFirstErrorTime = std::chrono::steady_clock::now();
        if (regPoll.mReadErrors != 1)
            regPoll.mReadErrors = 0;
    }

    // start sending MsgRegisterReadFailed if we cannot read register DefaultReadErrorCount times
    if (regPoll.mReadErrors > RegisterPoll::DefaultReadErrorCount) {
        MsgRegisterReadFailed msg(slaveId, regPoll.mRegisterType, regPoll.mRegister, regPoll.getCount());
        sendMessage(QueueItem::create(msg));
    }
}


}
