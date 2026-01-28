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

        virtual ModbusRegisters toModbus(const MqttValue& value, int registerCount) const {
            ModbusRegisters ret;

            mValues[0] = value.getDouble();

            double exprval = mExpression.value();


            typedef exprtk::results_context<double> results_context_t;
            const auto& results = mExpression.results();

            if (std::isnan(exprval)) {

                if (results.count() > 100)
                    throw ConvException("Too many values returned, max=100");

                if ((int)results.count() != registerCount)
                    throw ConvException("Got " + std::to_string(results.count()) + "values, need " + std::to_string(registerCount));

                typedef typename results_context_t::type_store_t type_t;

                for(std::size_t i = 0; i < results.count(); i++) {
                    type_t ts = results[i];
                    switch (ts.type) {
                        case type_t::e_scalar: {
                            double val = *(double*)(ts.data);
                            ret.appendValue(val);
                        } break;
                        case type_t::e_vector:
                            throw ConvException("Invalid list returned on position " + std::to_string(i));
                        break;
                        case type_t::e_string:
                            throw ConvException("Invalid string value returned on position " + std::to_string(i));
                        break;
                        case type_t::e_unknown:
                            throw ConvException("Unknown value type returned on position " + std::to_string(i));
                        break;
                    }
                }
            } else {
                ret.appendValue(exprval);
            }

            return ret;
         };


        virtual ConverterArgs getArgs() const {
            ConverterArgs ret;
            ret.add("expression", ConverterArgType::STRING, "");
            ret.add(ConverterArg::sPrecisionArgName, ConverterArgType::INT, ConverterArgValue::NO_PRECISION);
            return ret;
        }

        virtual void setArgValues(const ConverterArgValues& values) {
            mSymbolTable.add_function("int32",   int32);
            mSymbolTable.add_function("int32bs",   int32bs);
            mSymbolTable.add_function("uint32",  uint32);
            mSymbolTable.add_function("uint32bs",  uint32bs);
            mSymbolTable.add_function("flt32",   flt32);
            mSymbolTable.add_function("flt32bs", flt32bs);
            mSymbolTable.add_function("int16", int16);
            mSymbolTable.add_constants();

            char buf[16];
            for(uint16_t i = 0; i < mValues.size(); i++) {
                sprintf(buf, "R%d", i);
                mSymbolTable.add_variable(buf, mValues[i], false);
            }

            mSymbolTable.add_variable("M", mValues[0], false);

            mExpression.register_symbol_table(mSymbolTable);
            if (!mParser.compile(values["expression"].as_str(), mExpression)) {
                throw ConvException(std::string("Exprtk ") + mParser.error());
            }

            mPrecision = values[ConverterArg::sPrecisionArgName].as_int();
        }

        virtual ~ExprtkConverter() {
            mExpression.release();
            mSymbolTable.clear();
        }
    private:
        exprtk::symbol_table<double> mSymbolTable;
        exprtk::parser<double> mParser;
        exprtk::expression<double> mExpression;
        mutable std::vector<double> mValues;
        int mPrecision;

        static double int32(const double highRegister, const double lowRegister) {
            return ConverterTools::toNumber<int32_t>(highRegister, lowRegister, false);
        }

        static double int32bs(const double highRegister, const double lowRegister) {
            return ConverterTools::toNumber<int32_t>(highRegister, lowRegister, true);
        }

        static double uint32(const double highRegister, const double lowRegister) {
            return ConverterTools::toNumber<uint32_t>(highRegister, lowRegister, false);
        }

        static double uint32bs(const double highRegister, const double lowRegister) {
            return ConverterTools::toNumber<uint32_t>(highRegister, lowRegister, true);
        }

        static double flt32(const double highRegister, const double lowRegister) {
            return ConverterTools::toNumber<float>(highRegister, lowRegister, false);
        }

        static double flt32bs(const double highRegister, const double lowRegister) {
            return ConverterTools::toNumber<float>(highRegister, lowRegister, true);
        }

        static double int16(const double regValue) {
            uint16_t tmp = uint16_t(regValue);
            return (int16_t)tmp;
        }
};
