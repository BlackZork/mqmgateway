#pragma once

#include <map>
#include <vector>
#include <memory>
#include <chrono>

#include "logging.hpp"
#include "modbus_types.hpp"
#include "register_poll.hpp"

namespace modmqttd {

    class ModbusScheduler {
        public:
            void setPollSpecification(const std::map<int, std::vector<std::shared_ptr<RegisterPoll>>>& pRegisterMap) {
                mRegisterMap = pRegisterMap;
            }
            const std::map<int, std::vector<std::shared_ptr<RegisterPoll>>>& getPollSpecification() const {
                return mRegisterMap;
            }

            /**
             * Returns map of devices with list of registers, that
             * should be polled now.
             *
             * sets outDuration to time period that should be waited
             * for next poll to be done.
             *
             * */
            std::map<int, std::vector<std::shared_ptr<RegisterPoll>>> getRegistersToPoll(
                std::chrono::steady_clock::duration& outDuration,
                const std::chrono::time_point<std::chrono::steady_clock>& timePoint
            ) const;

            std::chrono::steady_clock::duration getMinPollTime() const;

            void remove(int pSlaveId, int pRegisterNumber, RegisterType pRegisterType);

            /**
             * Called after a one-shot RPC read completes successfully.
             * If the poll specification contains a register entry that covers
             * exactly the same address range (register number, type and count),
             * its read times are advanced with the real RPC execution times so
             * the scheduler does not issue a redundant poll within the refresh
             * window. A partial overlap (e.g. polled count=2, RPC count=1) is
             * intentionally left untouched - that poll must still run.
             */
            void notifyRpcRead(const RegisterPoll& pCompleted);

        private:
            std::map<int, std::vector<std::shared_ptr<RegisterPoll>>> mRegisterMap;
    };
}

