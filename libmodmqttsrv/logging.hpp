#pragma once

#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>

namespace modmqttd {

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

        static std::string get_pattern_prefix();
        static void init_logging(severity level);
    private:
        static void init_boost_logging(severity level);
        static void init_spdlog_logging(severity level);

};

}
