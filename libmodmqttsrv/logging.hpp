#pragma once

#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>

namespace modmqttd {

class Log {
    public:
        enum severity
        {
            critical,
            error,
            warn,
            info,
            debug,
            trace
        };

        static void init_logging(severity level);
};

}
