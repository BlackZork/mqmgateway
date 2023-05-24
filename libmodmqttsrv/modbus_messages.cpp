#include "modbus_messages.hpp"

namespace modmqttd {

void
MsgRegisterPollSpecification::merge(const MsgRegisterPoll& poll) {
    //TODO merge ranges
    // add new register poll or update refresh time on existing one
    std::vector<MsgRegisterPoll>::iterator reg_it = std::find_if(
        mRegisters.begin(), mRegisters.end(),
        [&poll](const MsgRegisterPoll& r) -> bool {
            return r.mRegister == poll.mRegister
                    && r.mRegisterType == poll.mRegisterType
                    && r.mSlaveId == poll.mSlaveId;
            }
    );

    if (reg_it == mRegisters.end()) {
        BOOST_LOG_SEV(log, Log::debug) << "Adding new register " << poll.mRegister <<
        " type=" << poll.mRegisterType << " refresh=" << poll.mRefreshMsec
        << " slaveId=" << poll.mSlaveId << " on network " << mNetworkName;
        mRegisters.push_back(poll);
    } else {
        //set the shortest poll period of all occurences in config file
        if (reg_it->mRefreshMsec > poll.mRefreshMsec) {
            reg_it->mRefreshMsec = poll.mRefreshMsec;
            BOOST_LOG_SEV(log, Log::debug) << "Setting refresh " << poll.mRefreshMsec << " on existing register " << poll.mRegister;
        }
    }
}

}
