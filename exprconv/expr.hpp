#pragma once

#include <exprtk.hpp>
#include "libmodmqttconv/convexception.hpp"
#include "libmodmqttconv/converter.hpp"

class ExprtkConverter : public IStateConverter {
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
            return MqttValue::fromDouble(ret);
        }

        virtual void setArgs(const std::vector<std::string>& args) {
            mSymbolTable.add_constants();

            char buf[8];
            for(int i = 0; i < mValues.size(); i++) {
                sprintf(buf, "R%d", i);
                mSymbolTable.add_variable(buf, mValues[i], false);
            }

            mExpression.register_symbol_table(mSymbolTable);
            mParser.compile(getArg(0, args), mExpression);
        }

        virtual ~ExprtkConverter() {}
    private:
        exprtk::symbol_table<double> mSymbolTable;
        exprtk::parser<double> mParser;
        exprtk::expression<double> mExpression;
        mutable std::vector<double> mValues;
};
