#include <fmt/std.h>
#include "modbus_messages.hpp"

#include <deque>
#include <spdlog/spdlog.h>

namespace modmqttd {

#if __cplusplus < 201703L
constexpr std::chrono::milliseconds MsgRegisterPoll::INVALID_REFRESH;
#endif

void
MsgRegisterPoll::merge(const MsgRegisterPoll& other) {
    ModbusAddressRange::merge(other);

    //set the shortest poll period
    if (mRefreshMsec == INVALID_REFRESH) {
        mRefreshMsec = other.mRefreshMsec;
    } else if (other.mRefreshMsec != INVALID_REFRESH && mRefreshMsec > other.mRefreshMsec) {
        mRefreshMsec = other.mRefreshMsec;
        spdlog::debug("Setting refresh {} on existing register", mRefreshMsec, mRegister);
    }
}

bool
MsgRegisterPoll::isSameAs(const MsgRegisterPoll& other) const {
    if (!ModbusAddressRange::isSameAs(other))
        return false;

    if (mSlaveId != other.mSlaveId)
        return false;

    return mRegister == other.mRegister && mCount == other.mCount;
}

struct RegRange {
    RegRange(const MsgRegisterPoll& poll)
        : mFirst(poll.mRegister), mLast(poll.mRegister + poll.mCount) {}
    int mFirst;
    int mLast;

    bool operator<(const RegRange& other) const {
        return mFirst < other.mFirst;
    }
};

typedef std::map<int, std::deque<MsgRegisterPoll>> RegTypeMap;
typedef std::map<int, RegTypeMap> SlaveMap;

bool registerNumberCompare(const MsgRegisterPoll& a, const MsgRegisterPoll& b) {
    return a.mRegister < b.mRegister;
}


void
MsgRegisterPollSpecification::group() {
    SlaveMap map;

    // ignore different refresh times
    // and use the shortest one
    for(auto& reg: mRegisters) {
        map[reg.mSlaveId][reg.mRegisterType].push_back(reg);
    }

    mRegisters.clear();

    for(auto& slave: map) {
        for(auto& regtype: slave.second) {
            auto& regs = regtype.second;
            sort(regs.begin(), regs.end(), registerNumberCompare);

            std::vector<MsgRegisterPoll> grouped(1, regs.front()); regs.pop_front();
            auto group_it = grouped.begin();
            while(!regs.empty()) {
                if (group_it->isConsecutiveOf(regs.front()))  {
                    group_it->merge(regs.front());
                } else {
                    group_it = grouped.insert(grouped.end(), regs.front());
                }
                regs.pop_front();
            }
            mRegisters.insert(mRegisters.end(), grouped.begin(), grouped.end());
        }
    }
}


void
MsgRegisterPollSpecification::merge(const MsgRegisterPoll& poll) {
    // remove all registers that overlaps with poll
    // merge them to one and push back
    std::vector<MsgRegisterPoll> overlaped;
    auto reg_it = mRegisters.begin();
    while(reg_it != mRegisters.end()) {
        if (poll.mSlaveId == reg_it->mSlaveId && poll.overlaps(*reg_it)) {
            overlaped.push_back(*reg_it);
            reg_it = mRegisters.erase(reg_it);
        } else {
            reg_it++;
        }
    }

    if (overlaped.empty()) {
        spdlog::debug("Adding new register {}.{} ({}) type={}, refresh={} on network {}",
            poll.mSlaveId,
            poll.mRegister,
            poll.mCount,
            (int)poll.mRegisterType,
            poll.mRefreshMsec,
            mNetworkName
        );
        mRegisters.push_back(poll);
    } else {
        mRegisters.push_back(poll);
        for(auto& reg: overlaped)
            mRegisters.back().merge(reg);
    }
}

}
