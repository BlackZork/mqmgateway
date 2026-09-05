#pragma once

#include <cmath>

#include "libmodmqttconv/converter.hpp"

#include "argtools.hpp"

class FloatConverter : public DataConverter {
    public:
        virtual MqttValue toMqtt(const ModbusRegisters& data) const {
            if (data.getCount() != REGISTER_COUNT) {
                throw ConvException("32-bit float needs " + std::to_string(REGISTER_COUNT) + " registers, got " + std::to_string(data.getCount()));
            }

            float val = ConverterTools::registersToFloatingPoint<float>(data.values(), mLowFirst, mSwapBytes);
            return MqttValue::fromDouble(val, mPrecision);
        }

        virtual ModbusRegisters toModbus(const MqttValue& value, int registerCount) const {
            if (registerCount != REGISTER_COUNT) {
                throw ConvException("32-bit float needs " + std::to_string(REGISTER_COUNT) + " registers, got " + std::to_string(registerCount));
            }

            return ConverterTools::floatingPointToRegisters<float>(static_cast<float>(value.getDouble()), mLowFirst, mSwapBytes, registerCount);
        };

        virtual ConverterArgs getArgs() const {
            ConverterArgs ret;
            ret.add(ConverterArg::sPrecisionArgName, ConverterArgType::INT, ConverterArgValue::NO_PRECISION);
            ret.add(ConverterArg::sLowFirstArgName, ConverterArgType::BOOL, false);
            ret.add(ConverterArg::sSwapBytesArgName, ConverterArgType::BOOL, false);
            return ret;
        };

        virtual void setArgValues(const ConverterArgValues& args) {
            mSwapBytes = RegisterOrderArgTools::getSwapBytes(args);
            mLowFirst = RegisterOrderArgTools::getLowFirst(args);
            mPrecision = args[ConverterArg::sPrecisionArgName].as_int();
        };

        virtual ~FloatConverter() {}
    private:
        static constexpr int REGISTER_COUNT = 2;

        int mPrecision = ConverterArgValue::NO_PRECISION;
        bool mLowFirst = false;
        bool mSwapBytes = false;
};
