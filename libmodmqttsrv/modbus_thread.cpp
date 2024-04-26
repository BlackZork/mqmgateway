#include "modbus_thread.hpp"

#include "debugtools.hpp"
#include "modmqtt.hpp"
#include "modbus_types.hpp"
#include "modbus_context.hpp"


namespace modmqttd {

void
ModbusThread::sendMessageFromModbus(moodycamel::BlockingReaderWriterQueue<QueueItem>& fromModbusQueue, const QueueItem& item) {
    fromModbusQueue.enqueue(item);
    modmqttd::notifyQueues();
}

ModbusThread::ModbusThread(
    moodycamel::BlockingReaderWriterQueue<QueueItem>& toModbusQueue,
    moodycamel::BlockingReaderWriterQueue<QueueItem>& fromModbusQueue)
    : mToModbusQueue(toModbusQueue),
      mFromModbusQueue(fromModbusQueue),
      mExecutor(fromModbusQueue)
{
}

void
ModbusThread::configure(const ModbusNetworkConfig& config) {
    mNetworkName = config.mName;
    mModbus = ModMqtt::getModbusFactory().getContext(config.mName);
    mModbus->init(config);
    mExecutor.init(mModbus);
    // if you experience read timeouts on RTU then increase
    // min_delay_before_poll config value
    mMinDelayBeforePoll = config.mMinDelayBeforePoll;
    BOOST_LOG_SEV(log, Log::info) << "Global minimum delay before poll set to " << std::chrono::duration_cast<std::chrono::milliseconds>(mMinDelayBeforePoll).count() << "ms";
}

void
ModbusThread::setPollSpecification(const MsgRegisterPollSpecification& spec) {
    std::map<int, std::vector<std::shared_ptr<RegisterPoll>>> registerMap;
    for(std::vector<MsgRegisterPoll>::const_iterator it = spec.mRegisters.begin();
        it != spec.mRegisters.end(); it++)
    {
        // do not poll a poll group declared in modbus config section
        // that was not merged with any mqtt register declaration
        if (it->mRefreshMsec != MsgRegisterPoll::INVALID_REFRESH) {
            std::shared_ptr<RegisterPoll> reg(new RegisterPoll(it->mRegister, it->mRegisterType, it->mCount, it->mRefreshMsec));
            std::map<int, ModbusSlaveConfig>::const_iterator slave_cfg = mSlaves.find(it->mSlaveId);
            if (slave_cfg != mSlaves.end())
                reg->updateSlaveConfig(slave_cfg->second);

            if (mMinDelayBeforePoll != std::chrono::milliseconds::zero())
                reg->mDelayBeforePoll = mMinDelayBeforePoll;

            registerMap[it->mSlaveId].push_back(reg);
        }
    }

    mScheduler.setPollSpecification(registerMap);
    BOOST_LOG_SEV(log, Log::debug) << "Poll specification set, got " << registerMap.size() << " slaves," << spec.mRegisters.size() << " registers to poll:";
    for (auto sit = registerMap.begin(); sit != registerMap.end(); sit++) {
        for (auto it = sit->second.begin(); it != sit->second.end(); it++) {

            BOOST_LOG_SEV(log, Log::debug)
            << mNetworkName
            << ", slave " << sit->first
            << ", register " << (*it)->mRegister << ":" << (*it)->mRegisterType
            << ", count=" << (*it)->getCount()
            << ", poll every " << std::chrono::duration_cast<std::chrono::milliseconds>((*it)->mRefresh).count() << "ms"
            << ", min delay " << std::chrono::duration_cast<std::chrono::milliseconds>((*it)->mDelayBeforePoll).count() << "ms";
        }
    }
    mExecutor.setupInitialPoll(registerMap);
    //now wait for MqttNetworkState(up)
}

void
ModbusThread::processWrite(const MsgRegisterValues& msg) {
    try {
        mModbus->writeModbusRegisters(msg);
        //send state change immediately if we
        //are polling this register
        auto& registers = mScheduler.getPollSpecification();
        std::map<int, std::vector<std::shared_ptr<RegisterPoll>>>::const_iterator slave = registers.find(msg.mSlaveId);
        if (slave != registers.end()) {
            int regNumber = msg.mRegisterNumber;
            std::vector<std::shared_ptr<RegisterPoll>>::const_iterator reg_it = std::find_if(
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
    do {
        if (item.isSameAs(typeid(ModbusNetworkConfig))) {
            configure(*item.getData<ModbusNetworkConfig>());
        } else if (item.isSameAs(typeid(MsgRegisterPollSpecification))) {
            setPollSpecification(*item.getData<MsgRegisterPollSpecification>());
            mGotRegisters = true;
        } else if (item.isSameAs(typeid(EndWorkMessage))) {
            BOOST_LOG_SEV(log, Log::debug) << "Got exit command";
            item.getData<EndWorkMessage>(); //free QueueItem memory
            mShouldRun = false;
        } else if (item.isSameAs(typeid(MsgRegisterValues))) {
            processWrite(*item.getData<MsgRegisterValues>());
        } else if (item.isSameAs(typeid(MsgMqttNetworkState))) {
            std::unique_ptr<MsgMqttNetworkState> netstate(item.getData<MsgMqttNetworkState>());
            mMqttConnected = netstate->mIsUp;
        } else if (item.isSameAs(typeid(ModbusSlaveConfig))) {
            //no per-slave config attributes defined yet
            updateSlaveConfig(*item.getData<ModbusSlaveConfig>());
        } else {
            BOOST_LOG_SEV(log, Log::error) << "Unknown message received, ignoring";
        }
        gotItem = mToModbusQueue.try_dequeue(item);
    } while(gotItem);
}

void
ModbusThread::sendMessage(const QueueItem& item) {
    sendMessageFromModbus(mFromModbusQueue, item);
}

void
ModbusThread::updateSlaveConfig(const ModbusSlaveConfig& pConfig) {
    std::pair<std::map<int, ModbusSlaveConfig>::iterator, bool> result = mSlaves.emplace(std::make_pair(pConfig.mAddress, pConfig));
    if(!result.second)  {
        result.first->second = pConfig;
    }

    auto& registers = mScheduler.getPollSpecification();
    std::map<int, std::vector<std::shared_ptr<RegisterPoll>>>::const_iterator slave_registers = registers.find(pConfig.mAddress);
    if (slave_registers != registers.end()) {
        for (auto it = slave_registers->second.begin(); it != slave_registers->second.end(); it++) {
            (*it)->mDelayBeforePoll = pConfig.mDelayBeforePoll;
        }
    }
}

std::string
constructIdleWaitMessage(const std::chrono::steady_clock::duration& idleWaitDuration) {
    std::stringstream out;
    if (idleWaitDuration == std::chrono::steady_clock::duration::zero()) {
        out << "Checking for incoming message";
    } else {
        out << "Waiting for messages";
        if (idleWaitDuration != std::chrono::steady_clock::duration::max()) {
            out << ", next poll in " <<  std::chrono::duration_cast<std::chrono::milliseconds>(idleWaitDuration).count() << "ms";
        }
    }
    return out.str();
}

void
ModbusThread::run() {
    try {
        BOOST_LOG_SEV(log, Log::debug) << "Modbus thread started";
        const int maxReconnectTime = 60;
        std::chrono::steady_clock::duration idleWaitDuration = std::chrono::steady_clock::duration::max();
        //after initial poll we want to call scheduler immediately
        std::chrono::steady_clock::duration timeToNextPoll = std::chrono::steady_clock::duration::zero();

        while(mShouldRun) {
            if (mModbus) {
                if (!mModbus->isConnected()) {
                    if (idleWaitDuration > std::chrono::seconds(maxReconnectTime))
                        idleWaitDuration = std::chrono::seconds(0);
                    BOOST_LOG_SEV(log, Log::info) << "modbus: connecting";
                    mModbus->connect();
                    if (mModbus->isConnected()) {
                        BOOST_LOG_SEV(log, Log::info) << "modbus: connected";
                        sendMessage(QueueItem::create(MsgModbusNetworkState(mNetworkName, true)));
                        // if modbus network was disconnected
                        // we need to refresh everything
                        //mNeedInitialPoll = true;
                        if (mGotRegisters)
                            mExecutor.setupInitialPoll(mScheduler.getPollSpecification());
                    }
                }

                if (mModbus->isConnected()) {
                    // start polling only if Mosquitto
                    // have succesfully connected to Mqtt broker
                    // to avoid growing mFromModbusQueue with queued register updates
                    // and if we already got the first MsgPollSpecification
                    if (mMqttConnected && mGotRegisters) {

                        if (mExecutor.allDone()) {
                            auto start = std::chrono::steady_clock::now();
                            timeToNextPoll = std::chrono::steady_clock::duration::max();
                            std::map<int, std::vector<std::shared_ptr<RegisterPoll>>> regsToPoll = mScheduler.getRegistersToPoll(timeToNextPoll, start);

                            if (regsToPoll.size()) {
                                mExecutor.setPollList(regsToPoll);
                                idleWaitDuration = std::chrono::steady_clock::duration::zero();
                            } else {
                                idleWaitDuration = timeToNextPoll;
                            }
                        } else {
                            idleWaitDuration = mExecutor.pollNext();
                            if (mExecutor.allDone()) {
                                if (mExecutor.isInitial()) {
                                    idleWaitDuration = std::chrono::steady_clock::duration::zero();
                                } else {
                                    idleWaitDuration = timeToNextPoll - mExecutor.getTotalPollDuration();
                                    if (idleWaitDuration < std::chrono::steady_clock::duration::zero()) {
                                        BOOST_LOG_SEV(log, Log::debug) << "Next poll is eariler than current poll time (" <<  std::chrono::duration_cast<std::chrono::milliseconds>(idleWaitDuration).count() << "ms)";
                                        idleWaitDuration = std::chrono::steady_clock::duration::zero();
                                    }
                                }
                            }
                        }
                    } else {
                        if (!mMqttConnected)
                            BOOST_LOG_SEV(log, Log::info) << "Waiting for mqtt network to become online";
                        if (!mGotRegisters)
                            BOOST_LOG_SEV(log, Log::debug) << "Waiting for poll specification message";

                        idleWaitDuration = std::chrono::steady_clock::duration::max();
                    }
                } else {
                    sendMessage(QueueItem::create(MsgModbusNetworkState(mNetworkName, false)));
                    if (idleWaitDuration < std::chrono::seconds(maxReconnectTime))
                        idleWaitDuration += std::chrono::seconds(5);
                };
            } else {
                //wait for modbus network config
                idleWaitDuration = std::chrono::steady_clock::duration::max();
            }

            //register polling can change mShouldRun flag, do not wait
            //for next poll if we are exiting
            if (mShouldRun) {
                QueueItem item;
                BOOST_LOG_SEV(log, Log::debug) << constructIdleWaitMessage(idleWaitDuration);
                if (!mToModbusQueue.wait_dequeue_timed(item, idleWaitDuration))
                    continue;
                dispatchMessages(item);
                while(mToModbusQueue.try_dequeue(item))
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
