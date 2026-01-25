#pragma once

#include <cmath>
#include "libmodmqttconv/converter.hpp"

class ScaleConverter : public DataConverter {
    public:
        virtual MqttValue toMqtt(const ModbusRegisters& data) const {
            double sourceValue = data.getValue(0);
            double targetValue = (mTargetScaleTo - mTargetScaleFrom)
                * (sourceValue - mSourceScaleFrom)/(mSourceScaleTo - mSourceScaleFrom)
                + mTargetScaleFrom;

            return MqttValue::fromDouble(targetValue, mPrecision);
        }

        virtual ConverterArgs getArgs() const {
            ConverterArgs ret;
            ret.add("src_from", ConverterArgType::DOUBLE, "");
            ret.add("src_to", ConverterArgType::DOUBLE, "");
            ret.add("tgt_from", ConverterArgType::DOUBLE, "");
            ret.add("tgt_to", ConverterArgType::DOUBLE, "");
            ret.add(ConverterArg::sPrecisionArgName, ConverterArgType::INT, ConverterArgValue::NO_PRECISION);
            return ret;
        };

        virtual void setArgValues(const ConverterArgValues& values) {
            mSourceScaleFrom = values["src_from"].as_double();
            mSourceScaleTo = values["src_to"].as_double();
            mTargetScaleFrom = values["tgt_from"].as_double();
            mTargetScaleTo = values["tgt_to"].as_double();
            mPrecision = values[ConverterArg::sPrecisionArgName].as_int();
        };

        virtual ~ScaleConverter() {}
    private:
        double mSourceScaleFrom;
        double mSourceScaleTo;
        double mTargetScaleFrom;
        double mTargetScaleTo;
        int mPrecision;
};
