#pragma once

#include <exprtk.hpp>
#include "libmodmqttconv/convexception.hpp"
#include "libmodmqttconv/converter.hpp"

class ExprtkConverter : public DataConverter {
    public:
        static const int MAX_REGISTERS = 10;

        ExprtkConverter() : mValues(MAX_REGISTERS, 0) {}

        virtual MqttValue toMqtt(const ModbusRegisters& data) const {

            if (data.getCount() > MAX_REGISTERS)
                throw new ConvException("Maximum " +std::to_string(MAX_REGISTERS) + " registers allowed");

            for(int i = 0; i < data.getCount(); i++) {
                mValues[i] =  data.getValue(i);
            }

            double ret = mExpression.value();

            if (mPrecision == 0)
                return MqttValue::fromInt(ret);

            return MqttValue::fromDouble(ret, mPrecision);
        }

        virtual void setArgs(const std::vector<std::string>& args) {
            mSymbolTable.add_function("int32",   int32);
            mSymbolTable.add_function("uint32",  uint32);
            mSymbolTable.add_function("flt32",   flt32);
            mSymbolTable.add_function("flt32be", flt32be);
            mSymbolTable.add_function("int16", int16);
            mSymbolTable.add_constants();

            char buf[8];
            for(int i = 0; i < mValues.size(); i++) {
                sprintf(buf, "R%d", i);
                mSymbolTable.add_variable(buf, mValues[i], false);
            }

            mExpression.register_symbol_table(mSymbolTable);
            mParser.compile(ConverterTools::getArg(0, args), mExpression);

            if (args.size() == 2)
                mPrecision = ConverterTools::getIntArg(1, args);
        }

        virtual ~ExprtkConverter() {}
    private:
        exprtk::symbol_table<double> mSymbolTable;
        exprtk::parser<double> mParser;
        exprtk::expression<double> mExpression;
        mutable std::vector<double> mValues;
        int mPrecision = -1;

        static double int32(const double highRegister, const double lowRegister) {
            return ConverterTools::toNumber<int32_t>(highRegister, lowRegister, true);
        }

        static double uint32(const double highRegister, const double lowRegister) {
            return ConverterTools::toNumber<uint32_t>(highRegister, lowRegister, true);
        }

        static double flt32(const double highRegister, const double lowRegister) {
            return ConverterTools::toNumber<float>(highRegister, lowRegister, true);
        }

        static double flt32be(const double highRegister, const double lowRegister) {
            return ConverterTools::toNumber<float>(highRegister, lowRegister);
        }

        static double int16(const double regValue) {
            return ConverterTools::toNumber<int16_t>(0, regValue);
        }
};
