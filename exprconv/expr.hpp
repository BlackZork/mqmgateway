#pragma once

#include <exprtk.hpp>
#include "libmodmqttconv/convexception.hpp"
#include "libmodmqttconv/converter.hpp"

class ExprtkConverter : public IStateConverter {
    public:
        virtual MqttValue toMqtt(const ModbusRegisters& data) const {

            exprtk::symbol_table<double> symbol_table;
            symbol_table.add_constants();

            char buf[8];
            if (data.getCount() > 999)
                throw new ConvException("Too many registers to convert");

            std::vector<double> values;

            for(int i = 0; i < data.getCount(); i++) {
                sprintf(buf, "R%d", i);
                values.push_back(data.getValue(i));
                symbol_table.add_variable(buf, values[i], true);
            }

            exprtk::parser<double> parser;
            exprtk::expression<double> expression;

            expression.register_symbol_table(symbol_table);

            parser.compile(mExpressionString, expression);

            double ret = expression.value();

            return MqttValue::fromDouble(ret);
        }

        virtual void setArgs(const std::vector<std::string>& args) {
            mExpressionString = getArg(0, args);
        }

        virtual ~ExprtkConverter() {}
    private:
        std::string mExpressionString;
};
