#pragma once

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/ringbuffer_sink.h>

/**
 * Captures what modmqttd logs while this object is alive, so a test can assert
 * on a diagnostic that has no other observable effect.
 *
 * Attaches to the default logger installed once by Log::init_logging(), and
 * lowers its level for the duration if MQM_TEST_LOGLEVEL is set quieter than
 * the messages under test. Both are restored on destruction.
 */
class LogCapture {
    public:
        LogCapture(spdlog::level::level_enum pLevel = spdlog::level::warn, size_t pMaxMessages = 256)
            : mLogger(spdlog::default_logger()),
              mSink(std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(pMaxMessages)),
              mRestoreLevel(mLogger->level()) {
            mSink->set_level(spdlog::level::trace);
            mLogger->sinks().push_back(mSink);
            if (mRestoreLevel > pLevel) {
                mLogger->set_level(pLevel);
            }
        }

        ~LogCapture() {
            mLogger->set_level(mRestoreLevel);
            std::vector<spdlog::sink_ptr>& sinks = mLogger->sinks();
            sinks.erase(std::remove(sinks.begin(), sinks.end(), mSink), sinks.end());
        }

        bool contains(const std::string& pText) const {
            const std::vector<std::string> lines(mSink->last_formatted());
            for (std::vector<std::string>::const_iterator it = lines.begin(); it != lines.end(); it++) {
                if (it->find(pText) != std::string::npos) {
                    return true;
                }
            }
            return false;
        }

        std::string text() const {
            std::string ret;
            const std::vector<std::string> lines(mSink->last_formatted());
            for (std::vector<std::string>::const_iterator it = lines.begin(); it != lines.end(); it++) {
                ret += *it;
            }
            return ret;
        }

    private:
        std::shared_ptr<spdlog::logger> mLogger;
        std::shared_ptr<spdlog::sinks::ringbuffer_sink_mt> mSink;
        spdlog::level::level_enum mRestoreLevel;
};
