#pragma once

#include "libmodmqttconv/converter.hpp"
#include "stdconv/int32.hpp"

class DivideConverter : public DataConverter {
    public:
        virtual MqttValue toMqtt(const ModbusRegisters& data) const {

            double val;
            if (data.getCount() == 1) {
                val = data.getValue(0);
            } else {
                val = ConverterTools::registersToInt32(data.values(), mLowFirst);
            }
            return MqttValue::fromDouble(doMath(val), mPrecision);
            //TODO uint16, uint32, float as separate data_type arg
        }

        virtual ModbusRegisters toModbus(const MqttValue& value, int registerCount) const {
            ModbusRegisters ret;
            //TODO uint16, uint32, float as separate data_type arg
            int32_t val = doMath(value.getDouble());
            return ConverterTools::int32ToRegisters(val, mLowFirst, registerCount);
        }

        virtual void setArgs(const std::vector<std::string>& args) {
            mDivisor = ConverterTools::getDoubleArg(0, args);
            if (args.size() > 1) {
                mPrecision = ConverterTools::getIntArg(1, args);
            }
            if (args.size() > 2) {
                std::string first_byte = ConverterTools::getArg(2, args);
                mLowFirst = (first_byte == "low_first");
            }
        }

        virtual ~DivideConverter() {}
    private:
        double mDivisor;
        int mPrecision = -1;
        bool mLowFirst = false;

        double doMath(double value) const {
            return value / mDivisor;
        }
};
