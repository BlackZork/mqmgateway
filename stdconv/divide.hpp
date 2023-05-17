#pragma once

#include <cmath>
#include "libmodmqttconv/converter.hpp"

class DivideConverter : public DataConverter {
    public:
        virtual MqttValue toMqtt(const ModbusRegisters& data) const {
            return MqttValue::fromDouble(doMath(data.getValue(0)));
        }

        virtual ModbusRegisters toModbus(const MqttValue& value, int registerCount) const {
            ModbusRegisters ret;
            int val = (int)doMath(value.getDouble());
            ret.appendValue(val);
            if (registerCount == 2) {
                ret.prependValue(val >> 16);
            }
            return ret;
        }

        virtual void setArgs(const std::vector<std::string>& args) {
            divider = ConverterTools::getDoubleArg(0, args);
            if (args.size() == 2)
                precision = ConverterTools::getIntArg(1, args);
        }

        virtual ~DivideConverter() {}
    private:
        double divider;
        int precision = 0;

        static double round(double val, int decimal_digits) {
            double divider = pow(10, decimal_digits);
            int dummy = (int)(val * divider);
            return dummy / divider;
        }

        double doMath(double value) const {
            double ret = value / divider;
            if (precision != 0)
                ret = round(ret, precision);
            return ret;
        }
};
