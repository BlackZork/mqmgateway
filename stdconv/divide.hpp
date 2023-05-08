#pragma once

#include <cmath>
#include "libmodmqttconv/converter.hpp"

class DivideConverter : public IStateConverter {
    public:
        virtual MqttValue toMqtt(const ModbusRegisters& data) const {
            double val = data.getValue(0) / divider;
            if (precision != 0)
                val = round(val, precision);

            return MqttValue::fromDouble(val);
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
};
