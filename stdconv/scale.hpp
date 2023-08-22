#pragma once

#include <cmath>
#include "libmodmqttconv/converter.hpp"

class ScaleConverter : public DataConverter {
    public:
        virtual MqttValue toMqtt(const ModbusRegisters& data) const {
            double sourceValue = data.getValue(0);
            double targetValue = (targetScaleTo - targetScaleFrom)
                * (sourceValue - sourceScaleFrom)/(sourceScaleTo - sourceScaleFrom)
                + targetScaleFrom;

            return MqttValue::fromDouble(targetValue, precision);
        }

        virtual void setArgs(const std::vector<std::string>& args) {
            sourceScaleFrom = ConverterTools::getDoubleArg(0, args);
            sourceScaleTo = ConverterTools::getDoubleArg(1, args);
            targetScaleFrom = ConverterTools::getDoubleArg(2, args);
            targetScaleTo = ConverterTools::getDoubleArg(3, args);

            if (args.size() == 5)
                precision = ConverterTools::getIntArg(4, args);
        }

        virtual ~ScaleConverter() {}
    private:
        double sourceScaleFrom;
        double sourceScaleTo;
        double targetScaleFrom;
        double targetScaleTo;
        int precision = 0;
};
