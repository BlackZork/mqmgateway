#pragma once

#include "../readerwriterqueue/readerwriterqueue.h"

#include "common.hpp"
#include "register_poll.hpp"
#include "modbus_context.hpp"
#include "queue_item.hpp"

namespace modmqttd {

class ModbusPoller {
    public:
        ModbusPoller(moodycamel::BlockingReaderWriterQueue<QueueItem>& fromModbusQueue);
        void init(const std::shared_ptr<IModbusContext>& modbus) { mModbus = modbus; }
        void doInitialPoll(const std::map<int, std::vector<std::shared_ptr<RegisterPoll>>>& pRegisters);
        bool allDone() const;
        void setPollList(const std::map<int, std::vector<std::shared_ptr<RegisterPoll>>>& pRegisters);
        std::chrono::steady_clock::duration pollOne();
    private:
        boost::log::sources::severity_logger<Log::severity> log;
        void pollRegisters(int slaveId, const std::vector<std::shared_ptr<RegisterPoll>>& registers, bool sendIfChanged);
        std::shared_ptr<IModbusContext> mModbus;
        moodycamel::BlockingReaderWriterQueue<QueueItem>& mFromModbusQueue;
        void sendMessage(const QueueItem& item);
        void handleRegisterReadError(int slaveId, RegisterPoll& regPoll, const char* errorMessage);
};

}
