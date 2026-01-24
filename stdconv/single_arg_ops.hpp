#pragma once

#include "double_register_converter.hpp"

class SingleArgMathConverter : public DoubleRegisterConverter {
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

        virtual ConverterArgs getArgs() const {
            ConverterArgs ret;
            ret.add("divisor", ConverterArgType::DOUBLE, "");
            ret.add(ConverterArg::sPrecisionArgName, ConverterArgType::INT, ConverterArgValue::NO_PRECISION);
            ret.add(ConverterArg::sLowFirstArgName, ConverterArgType::BOOL, false);
            ret.add(ConverterArg::sSwapBytesArgName, ConverterArgType::BOOL, false);
            return ret;
        }

        virtual void setArgValues(const ConverterArgValues& args) {
            mDoubleArg = args["divisor"].as_double();
            setSwapBytes(args);
            setLowFirst(args);
            mPrecision = args[ConverterArg::sPrecisionArgName].as_int();
        };


        virtual ~SingleArgMathConverter() {}
    protected:
        SingleArgMathConverter(int defaultPrecision) : mPrecision(defaultPrecision) {}

        double mDoubleArg;
        int mPrecision = MqttValue::NO_PRECISION;

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
