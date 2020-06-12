#pragma once

#include <cmath>
#include "libmodmqttconv/converter.hpp"

class ScaleConverter : public IStateConverter {
    public:
        virtual MqttValue toMqtt(const ModbusRegisters& data) const {
            double sourceValue = data.getValue(0);
            double targetValue = (targetScaleTo - targetScaleFrom)
                * (sourceValue - sourceScaleFrom)/(sourceScaleTo - sourceScaleFrom)
                + targetScaleFrom;

            if (precision != 0)
                targetValue = round(targetValue, precision);

            return MqttValue::fromDouble(targetValue);
        }

        virtual void setArgs(const std::vector<std::string>& args) {
            sourceScaleFrom = getDoubleArg(0, args);
            sourceScaleTo = getDoubleArg(1, args);
            targetScaleFrom = getDoubleArg(2, args);
            targetScaleTo = getDoubleArg(3, args);

            if (args.size() == 5)
                precision = getIntArg(4, args);
        }

        virtual ~ScaleConverter() {}
    private:
        double sourceScaleFrom;
        double sourceScaleTo;
        double targetScaleFrom;
        double targetScaleTo;
        int precision = 0;

        static double round(double val, int decimal_digits) {
            double divider = pow(10, decimal_digits);
            int dummy = (int)(val * divider);
            return dummy / divider;
        }
};
