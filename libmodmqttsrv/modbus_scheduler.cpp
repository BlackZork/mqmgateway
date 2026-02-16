#include "modbus_scheduler.hpp"
#include "modbus_types.hpp"

namespace modmqttd {

std::map<int, std::vector<std::shared_ptr<RegisterPoll>>>
ModbusScheduler::getRegistersToPoll(
    std::chrono::steady_clock::duration& outDuration,
    const std::chrono::time_point<std::chrono::steady_clock>& timePoint
) const {
    std::map<int, std::vector<std::shared_ptr<RegisterPoll>>> ret;

    outDuration = std::chrono::steady_clock::duration::max();

    for(std::map<int, std::vector<std::shared_ptr<RegisterPoll>>>::const_iterator slave = mRegisterMap.begin();
        slave != mRegisterMap.end(); slave++)
    {
        for(std::vector<std::shared_ptr<RegisterPoll>>::const_iterator reg_it = slave->second.begin();
            reg_it != slave->second.end(); reg_it++)
        {
            const RegisterPoll& reg = **reg_it;

            if (reg.mPublishMode == PublishMode::ONCE && reg.mLastReadOk)
                continue;

            auto time_passed = timePoint - reg.mLastRead;
            auto time_to_poll = reg.mRefresh;

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

std::chrono::steady_clock::duration
ModbusScheduler::getMinPollTime() const {
    std::chrono::steady_clock::duration ret = std::chrono::steady_clock::duration::max();
    for(std::map<int, std::vector<std::shared_ptr<RegisterPoll>>>::const_iterator slave = mRegisterMap.begin();
        slave != mRegisterMap.end(); slave++)
    {
        for(std::vector<std::shared_ptr<RegisterPoll>>::const_iterator reg_it = slave->second.begin();
            reg_it != slave->second.end(); reg_it++)
        {
            const RegisterPoll& reg = **reg_it;
            if (reg.mRefresh < ret)
                ret = reg.mRefresh;
        }
    }
    return ret;
}

void
ModbusScheduler::remove(int pSlaveId, int pRegisterNumber, RegisterType pRegisterType) {
    std::map<int, std::vector<std::shared_ptr<RegisterPoll>>>::iterator sit = mRegisterMap.find(pSlaveId);
    if (sit != mRegisterMap.end()) {
        auto rit = std::find_if(
            sit->second.begin(),
            sit->second.end(),
            [&pRegisterNumber, pRegisterType](const std::shared_ptr<RegisterPoll>& rpoll)
            -> bool { return rpoll->mRegister == pRegisterNumber && rpoll->mRegisterType == pRegisterType; }
        );

        if (rit != sit->second.end()) {
            sit->second.erase(rit);
            if (sit->second.empty())
                mRegisterMap.erase(sit);
        }
    }
}


}
