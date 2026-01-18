#include "modbus_scheduler.hpp"
#include "modbus_types.hpp"

namespace modmqttd {

boost::log::sources::severity_logger<Log::severity> ModbusScheduler::log;

std::map<int, std::vector<std::shared_ptr<RegisterPoll>>>
ModbusScheduler::getRegistersToPoll(
    std::chrono::steady_clock::duration& outDuration,
    const std::chrono::time_point<std::chrono::steady_clock>& timePoint
) {
    std::map<int, std::vector<std::shared_ptr<RegisterPoll>>> ret;

    //BOOST_LOG_SEV(log, Log::trace) << "initial outduration " << std::chrono::duration_cast<std::chrono::milliseconds>(outDuration).count();

    outDuration = std::chrono::steady_clock::duration::max();
    for(std::map<int, std::vector<std::shared_ptr<RegisterPoll>>>::const_iterator slave = mRegisterMap.begin();
        slave != mRegisterMap.end(); slave++)
    {
        for(std::vector<std::shared_ptr<RegisterPoll>>::const_iterator reg_it = slave->second.begin();
            reg_it != slave->second.end(); reg_it++)
        {
            const RegisterPoll& reg = **reg_it;

            auto time_passed = timePoint - reg.mLastRead;
            auto time_to_poll = reg.mRefresh;

            //BOOST_LOG_SEV(log, Log::trace) << "time passed: " << std::chrono::duration_cast<std::chrono::milliseconds>(time_to_poll).count();

            if (time_passed >= reg.mRefresh) {
                spdlog::trace("Register {}.{}  added, last read {} ago",
                     slave->first,
                     reg.mRegister,
                     std::chrono::duration_cast<std::chrono::milliseconds>(time_passed)
                );
                ret[slave->first].push_back(*reg_it);
            } else {
                time_to_poll = reg.mRefresh - time_passed;
            }

            if (outDuration > time_to_poll) {
                outDuration = time_to_poll;
                spdlog::trace("Wait duration set to {} as next poll for register {}.{}",
                    std::chrono::duration_cast<std::chrono::milliseconds>(time_to_poll),
                    slave->first,
                    reg.mRegister
                );
            }
        }
    }

    return ret;
}

std::shared_ptr<RegisterPoll>
ModbusScheduler::findRegisterPoll(const MsgRegisterValues& pValues) const {

    std::map<int, std::vector<std::shared_ptr<RegisterPoll>>>::const_iterator slave = mRegisterMap.find(pValues.mSlaveId);
    if (slave != mRegisterMap.end()) {
        std::vector<std::shared_ptr<RegisterPoll>>::const_iterator reg_it = std::find_if(
            slave->second.begin(), slave->second.end(),
            [&pValues](const std::shared_ptr<RegisterPoll>& item) -> bool { return item->overlaps(pValues); }
        );
        if (reg_it != slave->second.end()) {
            return *reg_it;
        }
    }

    return std::shared_ptr<RegisterPoll>();
}

}
