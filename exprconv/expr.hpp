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
                throw ConvException("Maximum " +std::to_string(MAX_REGISTERS) + " registers allowed");

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

            char buf[16];
            for(uint16_t i = 0; i < mValues.size(); i++) {
                sprintf(buf, "R%d", i);
                mSymbolTable.add_variable(buf, mValues[i], false);
            }

            mExpression.register_symbol_table(mSymbolTable);
            if (!mParser.compile(ConverterTools::getArg(0, args), mExpression)) {
                throw ConvException(std::string("Exprtk ") + mParser.error());
            }

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
            uint16_t tmp = uint16_t(regValue);
            return (int16_t)tmp;
        }
};
