#pragma once

#include <cmath>

#include "libmodmqttconv/converter.hpp"

#include "argtools.hpp"

class FloatConverter : public DataConverter {
    public:
        virtual int getExpectedRegisterCount() const { return 2; }

        virtual MqttValue toMqtt(const ModbusRegisters& pData) const {
            requireExpectedRegisterCount(pData.getCount(), "32-bit float");

            float val = ConverterTools::registersToFloatingPoint<float>(pData.values(), mLowFirst, mSwapBytes);
            return MqttValue::fromDouble(val, mPrecision);
        }

        virtual ModbusRegisters toModbus(const MqttValue& pValue, int pRegisterCount) const {
            requireExpectedRegisterCount(pRegisterCount, "32-bit float");

            return ConverterTools::floatingPointToRegisters<float>(static_cast<float>(pValue.getDouble()), mLowFirst, mSwapBytes, pRegisterCount);
        };

        virtual ConverterArgs getArgs() const {
            ConverterArgs ret;
            ret.add(ConverterArg::sPrecisionArgName, ConverterArgType::INT, ConverterArgValue::NO_PRECISION);
            ret.add(ConverterArg::sLowFirstArgName, ConverterArgType::BOOL, false);
            ret.add(ConverterArg::sSwapBytesArgName, ConverterArgType::BOOL, false);
            return ret;
        };

        virtual void setArgValues(const ConverterArgValues& pArgs) {
            mSwapBytes = RegisterOrderArgTools::getSwapBytes(pArgs);
            mLowFirst = RegisterOrderArgTools::getLowFirst(pArgs);
            mPrecision = pArgs[ConverterArg::sPrecisionArgName].as_int();
        };

        virtual ~FloatConverter() {}
    private:
        int mPrecision = ConverterArgValue::NO_PRECISION;
        bool mLowFirst = false;
        bool mSwapBytes = false;
};
