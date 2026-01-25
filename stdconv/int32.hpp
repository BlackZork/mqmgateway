#pragma once

#include <cmath>

#include "libmodmqttconv/converter.hpp"

#include "argtools.hpp"

class Int32Converter : public DataConverter {
    public:
        virtual MqttValue toMqtt(const ModbusRegisters& data) const {
            int32_t val = ConverterTools::registersToInt32(data.values(), mLowFirst, mSwapBytes);
            return MqttValue::fromInt(val);
        }

        virtual ModbusRegisters toModbus(const MqttValue& value, int registerCount) const {
            return ConverterTools::int32ToRegisters(value.getInt(), mLowFirst, mSwapBytes, registerCount);
        }

        virtual ConverterArgs getArgs() const {
            ConverterArgs ret;
            ret.add(ConverterArg::sLowFirstArgName, ConverterArgType::BOOL, false);
            ret.add(ConverterArg::sSwapBytesArgName, ConverterArgType::BOOL, false);
            return ret;
        }

        virtual void setArgValues(const ConverterArgValues& args) {
            mSwapBytes = DoubleRegisterArgTools::getSwapBytes(args);
            mLowFirst = DoubleRegisterArgTools::getLowFirst(args);
        };

        virtual ~Int32Converter() {}
    private:
        bool mLowFirst;
        bool mSwapBytes;
};
