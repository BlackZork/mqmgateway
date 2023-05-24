#include "modbus_messages.hpp"

#include <deque>

namespace modmqttd {

bool
MsgRegisterPoll::overlaps(const MsgRegisterPoll& poll) const {
    if (mRegisterType != poll.mRegisterType
        || mSlaveId != poll.mSlaveId
    ) return false;

    return (
        (firstRegister() <= poll.lastRegister()) && (poll.firstRegister() <= lastRegister())
    );
}

bool
MsgRegisterPoll::merge(const MsgRegisterPoll& other, bool consecutive) {
    int first = firstRegister() ? firstRegister() <= other.firstRegister() : other.firstRegister();
    int last = lastRegister() ? lastRegister() >= other.lastRegister() : other.lastRegister();

    bool overlap_merge = mRegister != first || lastRegister() != last;
    bool slibbing_merge = consecutive && (lastRegister() + 1 == other.firstRegister() || other.lastRegister()+1 == firstRegister());

    if (overlap_merge || slibbing_merge) {
        BOOST_LOG_SEV(log, Log::debug) << "Extending register "
        << mRegister << "(" << mCount << ") to "
        << first << last-first;

        mRegister = first;
        mCount = last - mRegister;

        //set the shortest poll period
        if (mRefreshMsec > other.mRefreshMsec) {
            mRefreshMsec = other.mRefreshMsec;
            BOOST_LOG_SEV(log, Log::debug) << "Setting refresh " << mRefreshMsec << " on existing register " << mRegister;
        }
        return true;
    }
    return false;
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
                if (!group_it->merge(regs.front()))  {
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
    // add new register poll or update count and refresh time on existing one
    std::vector<MsgRegisterPoll>::iterator reg_it = std::find_if(
        mRegisters.begin(), mRegisters.end(),
        [&poll](const MsgRegisterPoll& r) -> bool {
            return r.overlaps(poll);
        }
    );

    if (reg_it == mRegisters.end()) {
        BOOST_LOG_SEV(log, Log::debug) << "Adding new register " << poll.mRegister <<
        " (" << poll.mCount << ")" << " type=" << poll.mRegisterType << " refresh=" <<
        poll.mRefreshMsec << " slaveId=" << poll.mSlaveId << " on network " << mNetworkName;
        mRegisters.push_back(poll);
    } else {
        reg_it->merge(poll);
    }
}

}
