#include "modbus_messages.hpp"

#include <deque>

namespace modmqttd {

#if __cplusplus < 201703L
    constexpr std::chrono::milliseconds MsgRegisterPoll::INVALID_REFRESH;
#endif

bool
MsgRegisterPoll::overlaps(const MsgRegisterPoll& poll) const {
    if (mRegisterType != poll.mRegisterType
        || mSlaveId != poll.mSlaveId
    ) return false;

    return (
        (firstRegister() <= poll.lastRegister()) && (poll.firstRegister() <= lastRegister())
    );
}

void
MsgRegisterPoll::merge(const MsgRegisterPoll& other) {
    int first = firstRegister() <= other.firstRegister() ? firstRegister() : other.firstRegister();
    int last = lastRegister() >= other.lastRegister() ? lastRegister() : other.lastRegister();

    BOOST_LOG_SEV(log, Log::debug) << "Extending register "
    << mRegister << "(" << mCount << ") to "
    << first << "(" <<  last-first+1 << ")";

    mRegister = first;
    mCount = last - mRegister + 1;

    //set the shortest poll period
    if (mRefreshMsec == INVALID_REFRESH) {
        mRefreshMsec = other.mRefreshMsec;
    } else if (other.mRefreshMsec != INVALID_REFRESH && mRefreshMsec > other.mRefreshMsec) {
        mRefreshMsec = other.mRefreshMsec;
        BOOST_LOG_SEV(log, Log::debug) << "Setting refresh " << mRefreshMsec.count() << "ms on existing register " << mRegister;
    }
}

bool
MsgRegisterPoll::isConsecutiveOf(const MsgRegisterPoll& other) const {
    return (lastRegister() + 1 == other.firstRegister() || other.lastRegister()+1 == firstRegister());
}

bool
MsgRegisterPoll::isSameAs(const MsgRegisterPoll& other) const {
    if (mRegisterType != other.mRegisterType
        || mSlaveId != other.mSlaveId
    ) return false;

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
        if (poll.overlaps(*reg_it)) {
            overlaped.push_back(*reg_it);
            reg_it = mRegisters.erase(reg_it);
        } else {
            reg_it++;
        }
    }

    if (overlaped.empty()) {
        BOOST_LOG_SEV(log, Log::debug) << "Adding new register " << poll.mSlaveId << "." << poll.mRegister <<
        " (" << poll.mCount << ")" << " type=" << poll.mRegisterType << " refresh=" <<
        poll.mRefreshMsec.count() << " on network " << mNetworkName;
        mRegisters.push_back(poll);
    } else {
        mRegisters.push_back(poll);
        for(auto& reg: overlaped)
            mRegisters.back().merge(reg);
    }
}

}
