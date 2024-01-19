#include "modbus_thread.hpp"

#include "debugtools.hpp"
#include "modmqtt.hpp"
#include "modbus_types.hpp"
#include "modbus_context.hpp"


namespace modmqttd {

ModbusThread::ModbusThread(
    moodycamel::BlockingReaderWriterQueue<QueueItem>& toModbusQueue,
    moodycamel::BlockingReaderWriterQueue<QueueItem>& fromModbusQueue)
    : mToModbusQueue(toModbusQueue), mFromModbusQueue(fromModbusQueue)
{
}

void
ModbusThread::configure(const ModbusNetworkConfig& config) {
    mNetworkName = config.mName;
    mModbus = ModMqtt::getModbusFactory().getContext(config.mName);
    mModbus->init(config);
    // if you experience read timeouts on RTU then increase
    // min_delay_before_poll config value
    mMinDelayBeforePoll = config.mMinDelayBeforePoll;
    BOOST_LOG_SEV(log, Log::info) << "Minimum delay before poll set to " << std::chrono::duration_cast<std::chrono::milliseconds>(mMinDelayBeforePoll).count() << "ms";
}

void
ModbusThread::pollRegisters(int slaveId, const std::vector<std::shared_ptr<RegisterPoll>>& registers, bool sendIfChanged) {
    for(std::vector<std::shared_ptr<RegisterPoll>>::const_iterator reg_it = registers.begin();
        reg_it != registers.end(); reg_it++)
    {
        try {
            std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
            RegisterPoll& reg(**reg_it);
            std::vector<uint16_t> newValues(mModbus->readModbusRegisters(slaveId, reg));
            reg.mLastRead = std::chrono::steady_clock::now();

            std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
            BOOST_LOG_SEV(log, Log::debug) << "Register " << slaveId << "." << reg.mRegister << " (0x" << std::hex << slaveId << ".0x" << std::hex << reg.mRegister << ")"
                            << " polled in "  << std::dec << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms";

            if ((reg.getValues() != newValues) || !sendIfChanged || (reg.mReadErrors != 0)) {
                MsgRegisterValues val(slaveId, reg.mRegisterType, reg.mRegister, newValues);
                sendMessage(QueueItem::create(val));
                reg.update(newValues);
                reg.mReadErrors = 0;
                BOOST_LOG_SEV(log, Log::debug) << "Register " << slaveId << "." << reg.mRegister
                    << " values sent, data=" << DebugTools::registersToStr(reg.getValues());
            };
            //handle incoming write requests
            //in poll loop to avoid delays
            processCommands();
        } catch (const ModbusReadException& ex) {
            handleRegisterReadError(slaveId, **reg_it, ex.what());
        };
    };
};

void
ModbusThread::handleRegisterReadError(int slaveId, RegisterPoll& regPoll, const char* errorMessage) {
    // avoid flooding logs with register read error messages - log last error every 5 minutes
    regPoll.mReadErrors++;
    if (regPoll.mReadErrors == 1 || (std::chrono::steady_clock::now() - regPoll.mFirstErrorTime > RegisterPoll::DurationBetweenLogError)) {
        BOOST_LOG_SEV(log, Log::error) << regPoll.mReadErrors << " error(s) when reading register "
            << slaveId << "." << regPoll.mRegister << ", last error: " << errorMessage;
        regPoll.mFirstErrorTime = std::chrono::steady_clock::now();
        if (regPoll.mReadErrors != 1)
            regPoll.mReadErrors = 0;
    }

    // start sending MsgRegisterReadFailed if we cannot read register DefaultReadErrorCount times
    if (regPoll.mReadErrors > RegisterPoll::DefaultReadErrorCount) {
        MsgRegisterReadFailed msg(slaveId, regPoll.mRegisterType, regPoll.mRegister, regPoll.getCount());
        sendMessage(QueueItem::create(msg));
    }
}

void
ModbusThread::setPollSpecification(const MsgRegisterPollSpecification& spec) {
    for(std::vector<MsgRegisterPoll>::const_iterator it = spec.mRegisters.begin();
        it != spec.mRegisters.end(); it++)
    {
        // do not poll a poll group declared in modbus config section
        // that was not merged with any mqtt register declaration
        if (it->mRefreshMsec != MsgRegisterPoll::INVALID_REFRESH) {
            std::shared_ptr<RegisterPoll> reg(new RegisterPoll(it->mRegister, it->mRegisterType, it->mCount, it->mRefreshMsec));
            mRegisters[it->mSlaveId].push_back(reg);
        }
    }
    BOOST_LOG_SEV(log, Log::debug) << "Poll specification set, got " << mRegisters.size() << " slaves," << spec.mRegisters.size() << " registers to poll:";
    for (auto sit = mRegisters.begin(); sit != mRegisters.end(); sit++) {
        for (auto it = sit->second.begin(); it != sit->second.end(); it++) {

            BOOST_LOG_SEV(log, Log::debug)
            << mNetworkName
            << ", slave " << sit->first
            << ", register " << (*it)->mRegister << ":" << (*it)->mRegisterType
            << ", count " << (*it)->getCount()
            << ", poll every " <<  std::chrono::duration_cast<std::chrono::milliseconds>((*it)->mRefresh).count() << "ms";
        }
    }

    //now wait for MqttNetworkState(up)
}

void
ModbusThread::doInitialPoll() {
    BOOST_LOG_SEV(log, Log::debug) << "starting initial poll";
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    for(std::map<int, std::vector<std::shared_ptr<RegisterPoll>>>::const_iterator slave = mRegisters.begin();
        slave != mRegisters.end(); slave++)
    {
        pollRegisters(slave->first, slave->second, false);
    }
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    BOOST_LOG_SEV(log, Log::info) << "Initial poll done in " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms";
    mNeedInitialPoll = false;
}


bool
ModbusThread::hasRegisters() const {
    for(std::map<int, std::vector<std::shared_ptr<RegisterPoll>>>::const_iterator it = mRegisters.begin();
        it != mRegisters.end(); it++)
    {
        if (it->second.size() != 0)
            return true;
    }
    return false;
}

void
ModbusThread::processWrite(const MsgRegisterValues& msg) {
    try {
        mModbus->writeModbusRegisters(msg);
        //send state change immediately if we
        //are polling this register
        std::map<int, std::vector<std::shared_ptr<RegisterPoll>>>::iterator slave = mRegisters.find(msg.mSlaveId);
        if (slave != mRegisters.end()) {
            int regNumber = msg.mRegisterNumber;
            std::vector<std::shared_ptr<RegisterPoll>>::iterator reg_it = std::find_if(
                slave->second.begin(), slave->second.end(),
                [&regNumber](const std::shared_ptr<RegisterPoll>& item) -> bool { return regNumber == item->mRegister; }
            );
            if (reg_it != slave->second.end()) {
                sendMessage(QueueItem::create(msg));
            }
        }
    } catch (const ModbusWriteException& ex) {
        BOOST_LOG_SEV(log, Log::error) << "error writing register "
            << msg.mSlaveId << "." << msg.mRegisterNumber << ": " << ex.what();
        MsgRegisterWriteFailed msg(msg.mSlaveId, msg.mRegisterType, msg.mRegisterNumber, msg.mCount);
        sendMessage(QueueItem::create(msg));
    }
}

void
ModbusThread::dispatchMessages(const QueueItem& read) {
    QueueItem item(read);
    bool gotItem = true;
    // WARNING: dispatch loop can be run from inside pollRegisters loop
    // when processing commands
    // do not call pollRegisters when handing incoming messages!
    do {
        if (item.isSameAs(typeid(ModbusNetworkConfig))) {
            configure(*item.getData<ModbusNetworkConfig>());
        } else if (item.isSameAs(typeid(MsgRegisterPollSpecification))) {
            setPollSpecification(*item.getData<MsgRegisterPollSpecification>());
        } else if (item.isSameAs(typeid(EndWorkMessage))) {
            BOOST_LOG_SEV(log, Log::debug) << "Got exit command";
            mShouldRun = false;
        } else if (item.isSameAs(typeid(MsgRegisterValues))) {
            processWrite(*item.getData<MsgRegisterValues>());
        } else if (item.isSameAs(typeid(MsgMqttNetworkState))) {
            std::unique_ptr<MsgMqttNetworkState> netstate(item.getData<MsgMqttNetworkState>());
            mShouldPoll = netstate->mIsUp;
        } else if (item.isSameAs(typeid(ModbusSlaveConfig))) {
            //no per-slave config attributes defined yet
        } else {
            BOOST_LOG_SEV(log, Log::error) << "Unknown message received, ignoring";
        }
        gotItem = mToModbusQueue.try_dequeue(item);
    } while(gotItem);
}

void
ModbusThread::processCommands() {
    QueueItem item;
    while(mToModbusQueue.try_dequeue(item))
        dispatchMessages(item);
}

void
ModbusThread::sendMessage(const QueueItem& item) {
    mFromModbusQueue.enqueue(item);
    modmqttd::notifyQueues();
}

void
ModbusThread::run() {
    try {
        BOOST_LOG_SEV(log, Log::debug) << "Modbus thread started";
        const int maxReconnectTime = 60;
        std::chrono::steady_clock::duration waitDuration = std::chrono::steady_clock::duration::max();

        while(mShouldRun) {
            if (mModbus) {
                if (!mModbus->isConnected()) {
                    if (waitDuration > std::chrono::seconds(maxReconnectTime))
                        waitDuration = std::chrono::seconds(0);
                    BOOST_LOG_SEV(log, Log::info) << "modbus: connecting";
                    mModbus->connect();
                    if (mModbus->isConnected()) {
                        BOOST_LOG_SEV(log, Log::info) << "modbus: connected";
                        sendMessage(QueueItem::create(MsgModbusNetworkState(mNetworkName, true)));
                        mNeedInitialPoll = true;
                    }
                }

                // start polling only if Mosquitto
                // have succesfully connected to Mqtt broker
                // to avoid growing mFromModbusQueue with queued register updates
                if (mModbus->isConnected()) {
                    if (mShouldPoll) {
                        // if modbus network was disconnected
                        // we need to refresh everything
                        if (mNeedInitialPoll)
                            doInitialPoll();

                        //initial wait set to infinity, scheduler will adjust this value
                        //to time period for next poll
                        waitDuration = std::chrono::steady_clock::duration::max();

                        // note start time and find registers that need a refresh now
                        auto start = std::chrono::steady_clock::now();
                        std::map<int, std::vector<std::shared_ptr<RegisterPoll>>> toRefresh = mScheduler.getRegistersToPoll(mRegisters, waitDuration, start);
                        for(std::map<int, std::vector<std::shared_ptr<RegisterPoll>>>::const_iterator slave = toRefresh.begin();
                            slave != toRefresh.end(); slave++)
                        {
                            //this may call processCommands
                            pollRegisters(slave->first, slave->second);

                            if (slave != toRefresh.end() && mMinDelayBeforePoll != std::chrono::milliseconds(0))
                                std::this_thread::sleep_for(mMinDelayBeforePoll);
                        }
                        //decrease time to next poll by last poll time
                        if (toRefresh.size() != 0) {
                            auto pollDuration = std::chrono::steady_clock::now() - start;
                            waitDuration -= pollDuration;
                            if (waitDuration < std::chrono::steady_clock::duration::zero()) {
                                BOOST_LOG_SEV(log, Log::debug) << "Next poll is eariler than current poll time (" <<  std::chrono::duration_cast<std::chrono::milliseconds>(waitDuration).count() << "ms)";
                                waitDuration = std::chrono::steady_clock::duration::zero();
                            }
                        }
                    } else {
                        BOOST_LOG_SEV(log, Log::info) << "Waiting for mqtt network to become online";
                        waitDuration = std::chrono::steady_clock::duration::max();
                    }
                } else {
                    sendMessage(QueueItem::create(MsgModbusNetworkState(mNetworkName, false)));
                    if (waitDuration < std::chrono::seconds(maxReconnectTime))
                        waitDuration += std::chrono::seconds(5);
                };
            } else {
                //wait for poll specification
                waitDuration = std::chrono::steady_clock::duration::max();
            }

            //register polling can change mShouldRun flag, do not wait
            //for next poll if we are exiting
            if (mShouldRun) {
                QueueItem item;
                if (waitDuration <= std::chrono::steady_clock::duration::zero() && mMinDelayBeforePoll != std::chrono::milliseconds(0)) {
                    waitDuration = mMinDelayBeforePoll;
                }
                BOOST_LOG_SEV(log, Log::debug) << "Waiting " <<  std::chrono::duration_cast<std::chrono::milliseconds>(waitDuration).count() << "ms for messages";
                if (!mToModbusQueue.wait_dequeue_timed(item, waitDuration))
                    continue;
                dispatchMessages(item);
            }
        };
        if (mModbus && mModbus->isConnected())
            mModbus->disconnect();
        BOOST_LOG_SEV(log, Log::debug) << "Modbus thread " << mNetworkName << " ended";
    } catch (const std::exception& ex) {
        BOOST_LOG_SEV(log, Log::critical) << "Error in modbus thread " << mNetworkName << ": " << ex.what();
    } catch (...) {
        BOOST_LOG_SEV(log, Log::critical) << "Unknown error in modbus thread " << mNetworkName;
    }
}

}
