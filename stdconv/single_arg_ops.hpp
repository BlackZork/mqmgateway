#pragma once

#include "libmodmqttconv/converter.hpp"

class SingleArgMathConverter : public DataConverter {
    public:
        virtual MqttValue toMqtt(const ModbusRegisters& data) const {

            double val;
            if (data.getCount() == 1) {
                val = data.getValue(0);
            } else {
                val = ConverterTools::registersToInt32(data.values(), mLowFirst);
            }
            return MqttValue::fromDouble(doMath(val), mPrecision);
        }

        virtual ModbusRegisters toModbus(const MqttValue& value, int registerCount) const {
            ModbusRegisters ret;
            int32_t val = doMath(value.getDouble());
            return ConverterTools::int32ToRegisters(val, mLowFirst, registerCount);
        }

        virtual void setArgs(const std::vector<std::string>& args) {
            mDoubleArg = ConverterTools::getDoubleArg(0, args);
            if (args.size() > 1) {
                mPrecision = ConverterTools::getIntArg(1, args);
            }
            if (args.size() > 2) {
                std::string first_byte = ConverterTools::getArg(2, args);
                mLowFirst = (first_byte == "low_first");
            }
        }

        virtual ~SingleArgMathConverter() {}
    protected:
        SingleArgMathConverter(int defaultPrecision) : mPrecision(defaultPrecision) {}

        double mDoubleArg;
        int mPrecision;
        bool mLowFirst = false;

        virtual double doMath(double value) const = 0;
};


class DivideConverter : public SingleArgMathConverter {
    public:
        DivideConverter() : SingleArgMathConverter(MqttValue::NO_PRECISION) {}
        virtual ~DivideConverter() {}

    protected:
        double doMath(double value) const {
            return value / mDoubleArg;
        };
};


class MultiplyConverter : public SingleArgMathConverter {
    public:
        MultiplyConverter(): SingleArgMathConverter(0) {}
        virtual ~MultiplyConverter() {}

    protected:
        double doMath(double value) const {
            return value * mDoubleArg;
        };
};
