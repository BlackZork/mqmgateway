#include "logging.hpp"

#include "modbus_thread.hpp"
#include "debugtools.hpp"
#include "modmqtt.hpp"
#include "modbus_types.hpp"
#include "modbus_context.hpp"
#include "threadutils.hpp"

namespace modmqttd {

void
setCommandDelays(RegisterCommand& cmd, const std::shared_ptr<const std::chrono::milliseconds>& everyTime, const std::shared_ptr<const std::chrono::milliseconds>& onChange) {
    if (everyTime != nullptr)
        cmd.setDelayBeforeCommand(*everyTime);
    if (onChange != nullptr)
        cmd.setDelayBeforeFirstCommand(*onChange);
}

void
ModbusThread::sendMessageFromModbus(moodycamel::BlockingReaderWriterQueue<QueueItem>& fromModbusQueue, const QueueItem& item) {
    fromModbusQueue.enqueue(item);
    modmqttd::notifyQueues();
}

ModbusThread::ModbusThread(
    const std::string pNetworkName,
    moodycamel::BlockingReaderWriterQueue<QueueItem>& toModbusQueue,
    moodycamel::BlockingReaderWriterQueue<QueueItem>& fromModbusQueue)
    : mNetworkName(pNetworkName),
      mToModbusQueue(toModbusQueue),
      mFromModbusQueue(fromModbusQueue),
      mExecutor(fromModbusQueue, toModbusQueue)
{
}

void
ModbusThread::configure(const ModbusNetworkConfig& config) {
    if (mNetworkName != config.mName) {
        mNetworkName = config.mName;
        ThreadUtils::set_thread_name(mNetworkName.c_str());
    }
    mModbus = ModMqtt::getModbusFactory().getContext(config.mName);
    mModbus->init(config);
    mExecutor.init(mModbus);
    mWatchdog.init(config.mWatchdogConfig);

    if (config.hasDelayBeforeCommand())
        mDelayBeforeCommand = config.getDelayBeforeCommand();
    if (config.hasDelayBeforeFirstCommand())
        mDelayBeforeFirstCommand = config.getDelayBeforeFirstCommand();

    if (mDelayBeforeCommand != nullptr) {
        spdlog::info("Network default delay before every command set to {}",
            std::chrono::duration_cast<std::chrono::milliseconds>(*mDelayBeforeCommand)
        );
    }

    if (mDelayBeforeFirstCommand != nullptr) {
        spdlog::info("Network default delay when slave changes set to",
            std::chrono::duration_cast<std::chrono::milliseconds>(*mDelayBeforeFirstCommand)
        );
    }

    mMaxReadRetryCount = config.mMaxReadRetryCount;
    mMaxWriteRetryCount = config.mMaxWriteRetryCount;
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
            std::shared_ptr<RegisterPoll> reg(new RegisterPoll(it->mSlaveId, it->mRegister, it->mRegisterType, it->mCount, it->mRefreshMsec, it->mPublishMode));
            std::map<int, ModbusSlaveConfig>::const_iterator slave_cfg = mSlaves.find(reg->mSlaveId);

            setCommandDelays(*reg, mDelayBeforeCommand, mDelayBeforeFirstCommand);
            reg->setMaxRetryCounts(mMaxReadRetryCount, mMaxWriteRetryCount, true);

            if (slave_cfg != mSlaves.end()) {
                setCommandDelays(*reg, slave_cfg->second.getDelayBeforeCommand(), slave_cfg->second.getDelayBeforeFirstCommand());
                reg->setMaxRetryCounts(slave_cfg->second.mMaxReadRetryCount, slave_cfg->second.mMaxWriteRetryCount);
            }

            registerMap[reg->mSlaveId].push_back(reg);
        }
    }

    mScheduler.setPollSpecification(registerMap);
    spdlog::info("Poll specification set, got {} slaves, {} registers to poll:",
        registerMap.size(),
        spec.mRegisters.size()
    );
    for (auto sit = registerMap.begin(); sit != registerMap.end(); sit++) {
        for (auto it = sit->second.begin(); it != sit->second.end(); it++) {

            spdlog::info("slave {}, register {}:{} count={}, poll every {}, queue {}, min f_delay {}, min delay {}",
                sit->first,
                (*it)->mRegister,
                (int)(*it)->mRegisterType,
                (*it)->getCount(),
                std::chrono::duration_cast<std::chrono::milliseconds>((*it)->mRefresh),
                (*it)->mPublishMode == PublishMode::ON_CHANGE ? "on change" : "always",
                std::chrono::duration_cast<std::chrono::milliseconds>((*it)->getDelayBeforeFirstCommand()),
                std::chrono::duration_cast<std::chrono::milliseconds>((*it)->getDelayBeforeCommand())
            );
        }
    }
    mExecutor.setupInitialPoll(registerMap);
    //now wait for MqttNetworkState(up)
}

void
ModbusThread::processWrite(const std::shared_ptr<MsgRegisterValues>& msg) {
    auto cmd = std::shared_ptr<RegisterWrite>(new RegisterWrite(*msg));

    //TODO cache this setup

    cmd->mReturnMessage = msg;

    setCommandDelays(*cmd, mDelayBeforeCommand, mDelayBeforeFirstCommand);
    cmd->setMaxRetryCounts(mMaxReadRetryCount, mMaxWriteRetryCount, true);
    std::map<int, ModbusSlaveConfig>::const_iterator it = mSlaves.find(msg->mSlaveId);
    if (it != mSlaves.end()) {
        setCommandDelays(*cmd, it->second.getDelayBeforeCommand(), it->second.getDelayBeforeFirstCommand());
        cmd->setMaxRetryCounts(it->second.mMaxReadRetryCount, it->second.mMaxWriteRetryCount);
    }

    mExecutor.addWriteCommand(cmd);
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
        } else if (item.isSameAs(typeid(EndWorkMessage))) {
            spdlog::debug("Got exit command");
            item.getData<EndWorkMessage>(); //free QueueItem memory
            mShouldRun = false;
        } else if (item.isSameAs(typeid(MsgRegisterValues))) {
            processWrite(item.getData<MsgRegisterValues>());
        } else if (item.isSameAs(typeid(MsgMqttNetworkState))) {
            std::unique_ptr<MsgMqttNetworkState> netstate(item.getData<MsgMqttNetworkState>());
            mMqttConnected = netstate->mIsUp;
        } else if (item.isSameAs(typeid(ModbusSlaveConfig))) {
            //no per-slave config attributes defined yet
            updateFromSlaveConfig(*item.getData<ModbusSlaveConfig>());
        } else {
            spdlog::error("Unknown message received, ignoring");
        }
        gotItem = mToModbusQueue.try_dequeue(item);
    } while(gotItem);
}

void
ModbusThread::sendMessage(const QueueItem& item) {
    sendMessageFromModbus(mFromModbusQueue, item);
}

void
ModbusThread::updateFromSlaveConfig(const ModbusSlaveConfig& pConfig) {
    std::pair<std::map<int, ModbusSlaveConfig>::iterator, bool> result = mSlaves.emplace(std::make_pair(pConfig.mAddress, pConfig));
    if(!result.second)  {
        result.first->second = pConfig;
    }

    auto& registers = mScheduler.getPollSpecification();
    std::map<int, std::vector<std::shared_ptr<RegisterPoll>>>::const_iterator slave_registers = registers.find(pConfig.mAddress);
    if (slave_registers != registers.end()) {
        for (auto it = slave_registers->second.begin(); it != slave_registers->second.end(); it++) {
            setCommandDelays(**it, pConfig.getDelayBeforeCommand(), pConfig.getDelayBeforeFirstCommand());
            (*it)->setMaxRetryCounts(pConfig.mMaxReadRetryCount, pConfig.mMaxWriteRetryCount);
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
            out << " for  " <<  std::chrono::duration_cast<std::chrono::milliseconds>(idleWaitDuration).count() << "ms";
        }
    }
    return out.str();
}

void
ModbusThread::run() {
    try {
        spdlog::debug("Modbus thread started");
        const int maxReconnectTime = 60;
        std::chrono::steady_clock::duration idleWaitDuration = std::chrono::steady_clock::duration::max();
        std::chrono::steady_clock::time_point nextPollTimePoint = std::chrono::steady_clock::now();

        while(mShouldRun) {
            if (mModbus) {
                if (!mModbus->isConnected()) {
                    if (idleWaitDuration > std::chrono::seconds(maxReconnectTime))
                        idleWaitDuration = std::chrono::seconds(0);
                    spdlog::info("modbus: connecting");
                    mModbus->connect();
                    if (mModbus->isConnected()) {
                        spdlog::info("modbus: connected");
                        mWatchdog.reset();
                        sendMessage(QueueItem::create(MsgModbusNetworkState(mNetworkName, true)));
                        // if modbus network was disconnected
                        // we need to refresh everything
                        if (!mExecutor.isInitialPollInProgress()) {
                            mExecutor.setupInitialPoll(mScheduler.getPollSpecification());
                        }
                    }
                }

                if (mModbus->isConnected()) {
                    // start polling only if Mosquitto
                    // have succesfully connected to Mqtt broker
                    // to avoid growing mFromModbusQueue with queued register updates
                    // and if we already got the first MsgPollSpecification
                    if (mMqttConnected) {

                        auto now = std::chrono::steady_clock::now();
                        if (!mExecutor.isInitialPollInProgress() && nextPollTimePoint < now) {
                            std::chrono::steady_clock::duration schedulerWaitDuration;
                            std::map<int, std::vector<std::shared_ptr<RegisterPoll>>> regsToPoll = mScheduler.getRegistersToPoll(schedulerWaitDuration, now);
                            nextPollTimePoint = now + schedulerWaitDuration;
                            mExecutor.addPollList(regsToPoll);
                            spdlog::trace("Scheduling {} registers to execute, next schedule in {}",
                                regsToPoll.size(),
                                std::chrono::duration_cast<std::chrono::milliseconds>(schedulerWaitDuration)
                            );
                        }

                        if (mExecutor.allDone()) {
                            idleWaitDuration = (nextPollTimePoint - now);
                        } else {
                            idleWaitDuration = mExecutor.executeNext();
                            if (idleWaitDuration == std::chrono::steady_clock::duration::zero()) {
                                mWatchdog.inspectCommand(*mExecutor.getLastCommand());
                            }
                        }
                    } else {
                        if (!mMqttConnected)
                            spdlog::info("Waiting for mqtt network to become online");

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

            //dispatchMessages can change mShouldRun flag, do not wait
            //for next poll if we are exiting
            if (mShouldRun) {
                if (mModbus && mModbus->isConnected() && mWatchdog.isReconnectRequired()) {
                    if (mWatchdog.isDeviceRemoved()) {
                        spdlog::error("Device {} was removed, forcing reconnect");
                    } else {
                        spdlog::error("Cannot execute any command in last {}, reconnecting",
                            std::chrono::duration_cast<std::chrono::seconds>(mWatchdog.getCurrentErrorPeriod())
                        );
                    }
                    mWatchdog.reset();
                    mModbus->disconnect();
                    sendMessage(QueueItem::create(MsgModbusNetworkState(mNetworkName, false)));
                } else {
                    QueueItem item;
                    spdlog::trace(constructIdleWaitMessage(idleWaitDuration));
                    if (!mToModbusQueue.wait_dequeue_timed(item, idleWaitDuration))
                        continue;
                    dispatchMessages(item);
                    while(mToModbusQueue.try_dequeue(item))
                        dispatchMessages(item);
                }
            }
        };
        if (mModbus && mModbus->isConnected())
            mModbus->disconnect();
        spdlog::debug("Modbus thread  ended");
    } catch (const std::exception& ex) {
        spdlog::critical("Error in modbus thread: {}", ex.what());
    } catch (...) {
        spdlog::critical("Unknown error in modbus thread");
    }
}

}
