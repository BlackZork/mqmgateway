#pragma once

#include <fmt/std.h>
#include <spdlog/spdlog.h>

namespace modmqttd {

extern thread_local std::string g_thread_name;

class Log {
    public:
        enum severity
        {
            none = 0,
            critical = 1,
            error = 2,
            warn = 3,
            info = 4,
            debug = 5,
            trace = 6
        };

        static constexpr const char* loggerName = "modmqttd";
        static void init_logging(severity level);
    private:
        static void init_spdlog_logging(severity level);

};

}
