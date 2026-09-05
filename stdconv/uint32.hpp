#pragma once

#include <cmath>

#include "libmodmqttconv/converter.hpp"

#include "argtools.hpp"

class UInt32Converter : public DataConverter {
    public:
        virtual MqttValue toMqtt(const ModbusRegisters& data) const {
            uint32_t val = ConverterTools::registersToNumber<uint32_t>(data.values(), mLowFirst, mSwapBytes);
            return MqttValue::fromInt64(val);
        }

        virtual ModbusRegisters toModbus(const MqttValue& value, int registerCount) const {
            return ConverterTools::numberToRegisters<uint32_t>(static_cast<uint32_t>(value.getInt64()), mLowFirst, mSwapBytes, registerCount);
        }

        virtual ConverterArgs getArgs() const {
            ConverterArgs ret;
            ret.add(ConverterArg::sLowFirstArgName, ConverterArgType::BOOL, false);
            ret.add(ConverterArg::sSwapBytesArgName, ConverterArgType::BOOL, false);
            return ret;
        }

        virtual void setArgValues(const ConverterArgValues& args) {
            mSwapBytes = RegisterOrderArgTools::getSwapBytes(args);
            mLowFirst = RegisterOrderArgTools::getLowFirst(args);
        };

        virtual ~UInt32Converter() {}
    private:
        bool mLowFirst = false;
        bool mSwapBytes = false;
};
