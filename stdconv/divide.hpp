#pragma once

#include <cmath>
#include "libmodmqttconv/converter.hpp"
#include "stdconv/int32.hpp"

class DivideConverter : public DataConverter {
    public:
        virtual MqttValue toMqtt(const ModbusRegisters& data) const {

            if (data.getCount() == 1) {
                int16_t val = data.getValue(0);
                return MqttValue::fromDouble(doMath(val));
            }
            else {
                int32_t val = ConverterTools::registersToInt32(data, mLowFirst);
                return MqttValue::fromDouble(doMath(val));
            }
            //TODO uint16, uint32, float as separate data_type arg
        }

        virtual ModbusRegisters toModbus(const MqttValue& value, int registerCount) const {
            ModbusRegisters ret;
            //TODO uint16, uint32, float as separate data_type arg
            int32_t val = doMath(value.getDouble());
            return ConverterTools::int32ToRegisters(val, mLowFirst, registerCount);
        }

        virtual void setArgs(const std::vector<std::string>& args) {
            divider = ConverterTools::getDoubleArg(0, args);
            if (args.size() > 1) {
                precision = ConverterTools::getIntArg(1, args);
            }
            if (args.size() > 2) {
                std::string first_byte = ConverterTools::getArg(2, args);
                mLowFirst = (first_byte == "low_first");
            }
        }

        virtual ~DivideConverter() {}
    private:
        double divider;
        int precision = -1;
        bool mLowFirst = false;

        static double round(double val, int decimal_digits) {
            double divider = pow(10, decimal_digits);
            int dummy = (int)(val * divider);
            return dummy / divider;
        }

        double doMath(double value) const {
            double ret = value / divider;
            if (precision != -1)
                ret = round(ret, precision);
            return ret;
        }
};
