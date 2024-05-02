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

    //BOOST_LOG_SEV(log, Log::debug) << "initial outduration " << std::chrono::duration_cast<std::chrono::milliseconds>(outDuration).count();

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

            //BOOST_LOG_SEV(log, Log::debug) << "time passed: " << std::chrono::duration_cast<std::chrono::milliseconds>(time_to_poll).count();

            if (time_passed >= reg.mRefresh) {
                BOOST_LOG_SEV(log, Log::debug) << "Register " << slave->first << "." << reg.mRegister << " (0x" << std::hex << slave->first << ".0x" << std::hex << reg.mRegister << ")"
                                << " added, last read " << std::dec << std::chrono::duration_cast<std::chrono::milliseconds>(time_passed).count() << "ms ago";
                ret[slave->first].push_back(*reg_it);
            } else {
                time_to_poll = reg.mRefresh - time_passed;
            }

            if (outDuration > time_to_poll) {
                outDuration = time_to_poll;
                BOOST_LOG_SEV(log, Log::debug) << "Wait duration set to " << std::chrono::duration_cast<std::chrono::milliseconds>(time_to_poll).count()
                                << "ms as next poll for register " << slave->first << "." << reg.mRegister << " (0x" << std::hex << slave->first << ".0x" << std::hex << reg.mRegister << ")";
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
