#include <cstddef>
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

#include "logging.hpp"

namespace params = boost::log::keywords;
namespace sinks = boost::log::sinks;
namespace expr = boost::log::expressions;
namespace attrs = boost::log::attributes;

namespace modmqttd {

BOOST_LOG_ATTRIBUTE_KEYWORD(log_severity, "Severity", Log::severity)

std::ostream& operator<< (std::ostream& strm, Log::severity level)
{
    static const char* strings[] =
    {
        "CRITICAL",
        "ERROR",
        "WARNING",
        "INFO",
        "DEBUG",
        "TRACE"
    };

    if (static_cast< std::size_t >(level) < sizeof(strings) / sizeof(*strings))
        strm << strings[level];
    else
        strm << static_cast< int >(level);

    return strm;
}


void Log::init_logging(severity level) {
    typedef sinks::synchronous_sink< sinks::text_ostream_backend > text_sink;
    boost::shared_ptr< text_sink > sink = boost::make_shared< text_sink >();

    boost::shared_ptr< std::ostream > stream(&std::clog, boost::null_deleter());
    sink->locked_backend()->add_stream(stream);

    sink->set_formatter
    (
        expr::stream
            << expr::attr<boost::posix_time::ptime>("TimeStamp")
            << ": [" << log_severity << "]\t"
            << expr::smessage
    );

    sink->set_filter(log_severity <= level);

    boost::shared_ptr< boost::log::core > core = boost::log::core::get();

    core->add_sink(sink);
    //TODO remove timestamp, journalctl will add it anyway?
    core->add_global_attribute("TimeStamp", attrs::local_clock());

}

}
