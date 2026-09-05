#pragma once

#include "libmodmqttconv/converter.hpp"
#include "libmodmqttconv/convtools.hpp"

#include "argtools.hpp"

class UInt64Converter : public DataConverter {
    public:
        virtual int getExpectedRegisterCount() const { return 4; }

        virtual MqttValue toMqtt(const ModbusRegisters& pData) const {
            uint64_t val = ConverterTools::registersToNumber<uint64_t>(pData.values(), mLowFirst, mSwapBytes);
            return MqttValue::fromUInt64(val);
        }

        virtual ModbusRegisters toModbus(const MqttValue& pValue, int pRegisterCount) const {
            return ConverterTools::numberToRegisters<uint64_t>(pValue.getUInt64(), mLowFirst, mSwapBytes, pRegisterCount);
        }

        virtual ConverterArgs getArgs() const {
            ConverterArgs ret;
            ret.add(ConverterArg::sLowFirstArgName, ConverterArgType::BOOL, false);
            ret.add(ConverterArg::sSwapBytesArgName, ConverterArgType::BOOL, false);
            return ret;
        }

        virtual void setArgValues(const ConverterArgValues& pArgs) {
            mSwapBytes = RegisterOrderArgTools::getSwapBytes(pArgs);
            mLowFirst = RegisterOrderArgTools::getLowFirst(pArgs);
        };

        virtual ~UInt64Converter() {}

    private:
        bool mLowFirst = false;
        bool mSwapBytes = false;
};
