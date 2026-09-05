#pragma once

#include "libmodmqttconv/converter.hpp"
#include "libmodmqttconv/convtools.hpp"

#include "argtools.hpp"

class Float64Converter : public DataConverter {
    public:
        virtual int getExpectedRegisterCount() const { return 4; }

        virtual MqttValue toMqtt(const ModbusRegisters& pData) const {
            requireExpectedRegisterCount(pData.getCount(), "64-bit float");

            double val = ConverterTools::registersToFloatingPoint<double>(pData.values(), mLowFirst, mSwapBytes);
            return MqttValue::fromDouble(val, mPrecision);
        }

        virtual ModbusRegisters toModbus(const MqttValue& pValue, int pRegisterCount) const {
            requireExpectedRegisterCount(pRegisterCount, "64-bit float");

            return ConverterTools::floatingPointToRegisters<double>(pValue.getDouble(), mLowFirst, mSwapBytes, pRegisterCount);
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

        virtual ~Float64Converter() {}

    private:
        int mPrecision = ConverterArgValue::NO_PRECISION;
        bool mLowFirst = false;
        bool mSwapBytes = false;
};
