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

            if (precision == 0)
                return MqttValue::fromInt(ret);

            if (precision != -1)
                ret = round(ret, precision);
            return MqttValue::fromDouble(ret);
        }

        virtual void setArgs(const std::vector<std::string>& args) {
            mSymbolTable.add_function("int32be",   int32be);
            mSymbolTable.add_function("int32le",   int32le);
            mSymbolTable.add_function("uint32be",  uint32be);
            mSymbolTable.add_function("uint32le",  uint32le);
            mSymbolTable.add_function("flt32be",   flt32be);
            mSymbolTable.add_function("flt32le",   flt32le);
            mSymbolTable.add_function("flt32abcd", flt32abcd);
            mSymbolTable.add_function("flt32badc", flt32badc);
            mSymbolTable.add_function("flt32cdab", flt32cdab);
            mSymbolTable.add_function("flt32dcba", flt32dcba);
            mSymbolTable.add_constants();

            char buf[8];
            for(int i = 0; i < mValues.size(); i++) {
                sprintf(buf, "R%d", i);
                mSymbolTable.add_variable(buf, mValues[i], false);
            }

            mExpression.register_symbol_table(mSymbolTable);
            mParser.compile(ConverterTools::getArg(0, args), mExpression);

            if (args.size() == 2)
                precision = ConverterTools::getIntArg(1, args);
        }

        virtual ~ExprtkConverter() {}
    private:
        exprtk::symbol_table<double> mSymbolTable;
        exprtk::parser<double> mParser;
        exprtk::expression<double> mExpression;
        mutable std::vector<double> mValues;
        int precision = -1;

        static double round(double val, int decimal_digits) {
            double divider = pow(10, decimal_digits);
            int dummy = (int)(val * divider);
            return dummy / divider;
        }

        static double int32be(const double register1, const double register2) {
            return ConverterTools::toNumber<int32_t>(register1, register2);
        }

        static double int32le(const double register1, const double register2) {
            return ConverterTools::toNumber<int32_t>(register1, register2, true);
        }

        static double uint32be(const double register1, const double register2) {
            return ConverterTools::toNumber<uint32_t>(register1, register2);
        }

        static double uint32le(const double register1, const double register2) {
            return ConverterTools::toNumber<uint32_t>(register1, register2, true);
        }

        static double flt32be(const double register1, const double register2) {
            return ConverterTools::toNumber<float>(register1, register2);
        }

        static double flt32le(const double register1, const double register2) {
            return ConverterTools::toNumber<float>(register1, register2, true);
        }

        static double flt32abcd(const double register1, const double register2) {
            return ConverterTools::toNumber<float>(register1, register2);
        }

        static double flt32badc(const double register1, const double register2) {
            return ConverterTools::toNumber<float>(register1, register2, true);
        }

        static double flt32cdab(const double register1, const double register2) {
            return ConverterTools::toNumber<float>(register2, register1);
        }

        static double flt32dcba(const double register1, const double register2) {
            return ConverterTools::toNumber<float>(register2, register1, true);
        }
};
