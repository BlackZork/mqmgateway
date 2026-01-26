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

            std::shared_ptr<RegisterPoll> findRegisterPoll(const MsgRegisterValues& pValues) const;
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
            );
        private:
            std::map<int, std::vector<std::shared_ptr<RegisterPoll>>> mRegisterMap;
    };
}

