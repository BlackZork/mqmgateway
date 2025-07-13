#pragma once

#include "libmodmqttconv/converter.hpp"

class SingleArgMathConverter : public DataConverter {
    public:
        virtual MqttValue toMqtt(const ModbusRegisters& data) const {

            double val;
            if (data.getCount() == 1) {
                val = data.getValue(0);
            } else {
                val = ConverterTools::registersToInt32(data.values(), mLowFirst, mSwapBytes);
            }
            return MqttValue::fromDouble(doMath(val), mPrecision);
        }

        virtual ModbusRegisters toModbus(const MqttValue& value, int registerCount) const {
            ModbusRegisters ret;
            int32_t val = doMath(value.getDouble());
            return ConverterTools::int32ToRegisters(val, mLowFirst, mSwapBytes, registerCount);
        }

        virtual void setArgs(const std::vector<std::string>& args) {
            mDoubleArg = ConverterTools::getDoubleArg(0, args);
            switch (args.size()) {
                case 4: {
                    std::string swapBytes = ConverterTools::getArg(3, args);
                    mSwapBytes = (swapBytes == "swap_bytes");
                }
                case 3: {
                    std::string firstByte = ConverterTools::getArg(2, args);
                    mLowFirst = (firstByte == "low_first");
                }
                case 2: {
                    mPrecision = ConverterTools::getIntArg(1, args);
                }
            }
        }

        virtual ~SingleArgMathConverter() {}
    protected:
        SingleArgMathConverter(int defaultPrecision) : mPrecision(defaultPrecision) {}

        double mDoubleArg;
        int mPrecision = MqttValue::NO_PRECISION;
        bool mLowFirst = false;
        bool mSwapBytes = false;

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
