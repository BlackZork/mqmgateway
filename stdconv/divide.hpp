#pragma once

#include <cmath>
#include "libmodmqttconv/converter.hpp"

class DivideConverter : public DataConverter {
    public:
        virtual MqttValue toMqtt(const ModbusRegisters& data) const {
            int high = 0, low = 0;
            if (data.getCount() > 1) {
                high = mHighByte;
                low = mLowByte;
            }
            int32_t val = data.getValue(high);
            if (data.getCount() > 1) {
                val = val << 16;
                val += data.getValue(low);
            }

            return MqttValue::fromDouble(doMath(val));
        }

        virtual ModbusRegisters toModbus(const MqttValue& value, int registerCount) const {
            ModbusRegisters ret;
            int val = (int)doMath(value.getDouble());
            ret.appendValue(val);
            if (registerCount == 2) {
                if (mHighByte == 0)
                    ret.prependValue(val >> 16);
                else
                    ret.appendValue(val >> 16);
            }
            return ret;
        }

        virtual void setArgs(const std::vector<std::string>& args) {
            divider = ConverterTools::getDoubleArg(0, args);
            if (args.size() > 1) {
                precision = ConverterTools::getIntArg(1, args);
            }
            if (args.size() > 2) {
                std::string first_byte = ConverterTools::getArg(2, args);
                if (first_byte == "low_first") {
                    mLowByte = 0;
                    mHighByte = 1;
                }
            }
        }

        virtual ~DivideConverter() {}
    private:
        double divider;
        int precision = -1;
        int8_t mLowByte = 1;
        int8_t mHighByte = 0;

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
