#include <cstddef>
#include <spdlog/sinks/stdout_sinks.h>
#include <sys/stat.h>
#include <string>
#include <ostream>
#include <fstream>
#include <iomanip>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/attributes/clock.hpp>
#include <boost/parameter/keyword.hpp>
#include <boost/log/sinks.hpp>
#include <boost/core/null_deleter.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <spdlog/spdlog.h>

#include "logging.hpp"

namespace sinks = boost::log::sinks;
namespace expr = boost::log::expressions;
namespace attrs = boost::log::attributes;

namespace modmqttd {

BOOST_LOG_ATTRIBUTE_KEYWORD(log_severity, "Severity", Log::severity)

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

void Log::init_boost_logging(severity level) {
    if (level == modmqttd::Log::none) {
       boost::log::core::get()->set_logging_enabled(false);
       return;
    }
    typedef sinks::synchronous_sink< sinks::text_ostream_backend > text_sink;
    boost::shared_ptr< text_sink > sink = boost::make_shared< text_sink >();

    boost::shared_ptr< std::ostream > stream(&std::clog, boost::null_deleter());
    sink->locked_backend()->add_stream(stream);

    boost::log::formatter formatter;

    bool log_timestamp = true;

    //check JOURNAL_STREAM indoe vs stderr inode
    const char* js = std::getenv("JOURNAL_STREAM");
    if (js != nullptr) {
        std::string jstr(js);
        size_t t = jstr.find(':');
        if (jstr.size() > 2 &&  t > 0) {
            std::string env_inode(jstr.substr(t+1));
            if (!env_inode.empty()) {
                struct stat file_stat;
                int ret, inode;
                ret = fstat (2, &file_stat);
                if (ret >= 0) {
                    inode = file_stat.st_ino;
                    if (std::to_string(inode) == env_inode)
                        log_timestamp = false;
                }
            };
        }
    }

    auto ts_format = expr::stream << expr::attr<boost::posix_time::ptime>("TimeStamp") << ": ";
    auto log_format = expr::stream << "[" << log_severity << "] " << expr::smessage;

    if (log_timestamp) {
        //TODO how to use log_format ?
        auto format = ts_format << "[" << log_severity << "] " << expr::smessage;
        formatter = format;
    } else {
        formatter = log_format;
    }

    sink->set_formatter
    (
        formatter
    );

    sink->set_filter(log_severity <= level);

    boost::shared_ptr< boost::log::core > core = boost::log::core::get();

    core->add_sink(sink);
    //TODO remove timestamp, journalctl will add it anyway?
    core->add_global_attribute("TimeStamp", attrs::local_clock());
}

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

std::string
Log::get_pattern_prefix() {
    std::string ret;

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
                        ret = "%Y-%m-%d %H:%M:%S.%e";
                }
            };
        }
    }
    return ret;
}

void Log::init_spdlog_logging(severity level) {
    if (level == Log::none) {
        spdlog::set_level(spdlog::level::off);
        return;
    }

    try {
        // create stderr sink (thread-safe)
        auto sink = std::make_shared<spdlog::sinks::stderr_sink_mt>();
        auto logger = std::make_shared<spdlog::logger>("modmqttd", sink);
        spdlog::set_default_logger(logger);

        bool log_timestamp = true;
        const char* js = std::getenv("JOURNAL_STREAM");
        if (js != nullptr) {
            std::string jstr(js);
            size_t t = jstr.find(':');
            if (jstr.size() > 2 &&  t > 0) {
                std::string env_inode(jstr.substr(t+1));
                if (!env_inode.empty()) {
                    struct stat file_stat;
                    int ret, inode;
                    ret = fstat (2, &file_stat);
                    if (ret >= 0) {
                        inode = file_stat.st_ino;
                        if (std::to_string(inode) == env_inode)
                            log_timestamp = false;
                    }
                };
            }
        }

        if (log_timestamp) {
            // include milliseconds and colored level tag
            spdlog::set_pattern("%Y-%m-%d %H:%M:%S.%e [%^%l%$] %v");
        } else {
            // leave timestamp to systemd/journald
            spdlog::set_pattern("[%l] %v");
        }

        spdlog::set_level(to_spdlog_level(level));
        spdlog::flush_on(spdlog::level::info);
    } catch (const std::exception&) {
        // fallback: set global level only
        spdlog::set_level(to_spdlog_level(level));
    }
}

void Log::init_logging(severity level) {
    init_boost_logging(level);
    init_spdlog_logging(level);
}

}
