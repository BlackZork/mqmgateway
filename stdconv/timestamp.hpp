#include <boost/config.hpp> // for BOOST_SYMBOL_EXPORT
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/conversion.hpp>
#include "libmodmqttconv/converterplugin.hpp"

class TimestampGenerator : public DataConverter {
    public:
        //called by modmqttd to set coverter arguments
        virtual void setArgs(const std::vector<std::string>& args) {
            if (args.size() == 0) {
                format = "";
                return;
            }
            format = ConverterTools::getArg(0, args);
        }

        // Conversion from modbus registers to mqtt value
        // Used when converter is defined for state topic
        // ModbusRegisters contains one register or as many as
        // configured in unnamed register list.
        virtual MqttValue toMqtt(const ModbusRegisters& data) const {
            boost::posix_time::ptime timestamp = boost::posix_time::second_clock::local_time();

            if (format == "") {
                tm tm = boost::posix_time::to_tm(timestamp);
                return MqttValue::fromInt64(mktime(&tm));
            }

            boost::posix_time::time_facet* facet = new boost::posix_time::time_facet();
            facet->format(format.c_str());
            std::stringstream stream;
            stream.imbue(std::locale(std::locale::classic(), facet));

            stream << timestamp;

            return MqttValue::fromString(stream.str());
        }

        // Conversion from mqtt value to modbus register data
        // Used when converter is defined for command topic
        virtual ModbusRegisters toModbus(const MqttValue& value, int registerCount) const {
            ModbusRegisters ret;
            return ret;
        }

        virtual ~TimestampGenerator() {}
    private:
      std::string format;
};
