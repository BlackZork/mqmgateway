#pragma once

#include <cmath>
#include "libmodmqttconv/convexception.hpp"
#include "double_register_converter.hpp"

class FloatConverter : public DoubleRegisterConverter {
    public:
        virtual MqttValue toMqtt(const ModbusRegisters& data) const {
            if (data.getCount() < 2)
                throw ConvException("Cannot read 32-bit float from single register");

            char hb = 0; char lb = 1;
            if (mLowFirst) {
                hb = 1; lb = 0;
            }

            float val = ConverterTools::toNumber<float>(data.getValue(hb), data.getValue(lb), mSwapBytes);
            return MqttValue::fromDouble(val, mPrecision);
        }

        virtual ModbusRegisters toModbus(const MqttValue& value, int registerCount) const {
            if (registerCount < 2)
                throw ConvException("Cannot store float in single register");

            assert(sizeof(float) == sizeof(int32_t));
            union {
                int32_t out_value;
                float in_value;
            } CastData;

            CastData.in_value = value.getDouble();

            std::vector<uint16_t> regdata(
                ConverterTools::int32ToRegisters(CastData.out_value, mLowFirst, mSwapBytes, registerCount)
            );
            return ModbusRegisters(regdata);
        };

        virtual ConverterArgs getArgs() const {
            ConverterArgs ret;
            ret.add(ConverterArg::sPrecisionArgName, ConverterArgType::INT, ConverterArgValue::NO_PRECISION);
            ret.add(ConverterArg::sLowFirstArgName, ConverterArgType::BOOL, false);
            ret.add(ConverterArg::sSwapBytesArgName, ConverterArgType::BOOL, false);
            return ret;
        };

        virtual void setArgValues(const ConverterArgValues& args) {
            setSwapBytes(args);
            setLowFirst(args);
            mPrecision = args[ConverterArg::sPrecisionArgName].as_int();
        };

        virtual ~FloatConverter() {}
    private:
        int mPrecision;
};
