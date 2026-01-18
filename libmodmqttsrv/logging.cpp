#include <cstddef>
#include <spdlog/common.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <sys/stat.h>
#include <string>
#include <ostream>
#include <fstream>
#include <iomanip>
#include <boost/parameter/keyword.hpp>
#include <boost/core/null_deleter.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <spdlog/pattern_formatter.h>

#include <spdlog/spdlog.h>

#include "defs.h"
#include "logging.hpp"

namespace modmqttd {

thread_local std::string g_thread_name;


std::ostream& operator<< (std::ostream& strm, Log::severity level)
{
    static const char* strings[] =
    {
        "NONE",
        "CRITICAL",
        "ERROR",
        "WARN ",
        "INFO ",
        "DEBUG",
        "TRACE"
    };

    if (static_cast< std::size_t >(level) < sizeof(strings) / sizeof(*strings))
        strm << strings[level];
    else
        strm << static_cast< int >(level);

    return strm;
}

bool
log_timestamp() {
    bool ret = true;

    const char* js = std::getenv("JOURNAL_STREAM");
    if (js != nullptr) {
        std::string jstr(js);
        size_t t = jstr.find(':');
        if (jstr.size() > 2 &&  t > 0) {
            std::string env_inode(jstr.substr(t+1));
            if (!env_inode.empty()) {
                struct stat file_stat;
                int code, inode;
                code = fstat (2, &file_stat);
                if (code >= 0) {
                    inode = file_stat.st_ino;
                    if (std::to_string(inode) == env_inode)
                        ret = false;
                }
            };
        }
    }
    return ret;
}

// custom flag prints cached name
struct thread_name_flag : public spdlog::custom_flag_formatter {
    void format(const spdlog::details::log_msg &, const std::tm &, spdlog::memory_buf_t &dest) override {
        if (!g_thread_name.empty()) {
            fmt::format_to(std::back_inserter(dest), "{}", g_thread_name);
        } else {
            auto hid = std::hash<std::thread::id>{}(std::this_thread::get_id());
            fmt::format_to(std::back_inserter(dest), "tid:{}", hid);
        }
    }
    std::unique_ptr<custom_flag_formatter> clone() const override {
        return spdlog::details::make_unique<thread_name_flag>();
    }
};

struct uppercase_level_flag : public spdlog::custom_flag_formatter {
    void format(const spdlog::details::log_msg &msg, const std::tm &, spdlog::memory_buf_t &dest) override {
        const char* name = nullptr;
        switch (msg.level) {
            case spdlog::level::trace:    name = "TRACE"; break;
            case spdlog::level::debug:    name = "DEBUG"; break;
            case spdlog::level::info:     name = "INFO "; break;
            case spdlog::level::warn:     name = "WARN "; break;
            case spdlog::level::err:      name = "ERROR"; break;
            case spdlog::level::critical: name = "CRIT "; break;
            case spdlog::level::off:      name = "OFF  "; break;
            default:                      name = "???  "; break;
        }
        fmt::format_to(std::back_inserter(dest), "{}", name);
    }

    std::unique_ptr<custom_flag_formatter> clone() const override {
        return spdlog::details::make_unique<uppercase_level_flag>();
    }
};

spdlog::level::level_enum
to_spdlog_level(Log::severity s) {
    switch(s) {
        case Log::none:     return spdlog::level::off;
        case Log::critical: return spdlog::level::critical;
        case Log::error:    return spdlog::level::err;
        case Log::warn:     return spdlog::level::warn;
        case Log::info:     return spdlog::level::info;
        case Log::debug:    return spdlog::level::debug;
        case Log::trace:    return spdlog::level::trace;
        default:            return spdlog::level::info;
    }
}

void Log::init_spdlog_logging(severity level) {
    if (level == Log::none) {
        spdlog::set_level(spdlog::level::off);
        return;
    }

    try {
        // create stderr sink (thread-safe)
        auto sink = std::make_shared<spdlog::sinks::stderr_sink_mt>();
        auto logger = std::make_shared<spdlog::logger>(loggerName, sink);
        spdlog::set_default_logger(logger);

        auto formatter = std::make_unique<spdlog::pattern_formatter>();
        formatter->add_flag<thread_name_flag>('T');
        formatter->add_flag<uppercase_level_flag>('l');

        std::string pattern("[%l] %t %v");
        #ifdef HAVE_PTHREAD_SETNAME_NP
            int pos = pattern.find("%t");
            if (pos != std::string::npos) {
                pattern.replace(pos, 2, "%T");
            }
        #endif

        if (log_timestamp()) {
            // include milliseconds and colored level tag
            formatter->set_pattern("%Y-%m-%d %H:%M:%S.%f " + pattern);
        } else {
            // leave timestamp to systemd/journald
            formatter->set_pattern(pattern);
        }

        spdlog::set_formatter(std::move(formatter));
        spdlog::set_level(to_spdlog_level(level));
    } catch (const std::exception&) {
        // fallback: set global level only
        spdlog::set_level(to_spdlog_level(level));
    }
}

void Log::init_logging(severity level) {
    init_spdlog_logging(level);
}

}
