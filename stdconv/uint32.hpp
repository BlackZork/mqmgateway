#pragma once

#include <cmath>

#include "libmodmqttconv/converter.hpp"

#include "argtools.hpp"

class UInt32Converter : public DataConverter {
    public:
        virtual MqttValue toMqtt(const ModbusRegisters& data) const {
           int high = 0, low = 0;
            if (data.getCount() > 1) {
                high = mHighByte;
                low = mLowByte;
            }
            uint32_t val = data.getValue(high);
            if (data.getCount() > 1) {
                val = val << 16;
                val += data.getValue(low);
            }
            return MqttValue::fromInt64(val);
        }

        virtual ModbusRegisters toModbus(const MqttValue& value, int registerCount) const {
            ModbusRegisters ret;
            uint32_t val = value.getInt64();
            ret.appendValue(val);
            if (registerCount == 2) {
                if (mHighByte == 0)
                    ret.prependValue(val >> 16);
                else
                    ret.appendValue(val >> 16);
            }
            return ret;
        }

        virtual ConverterArgs getArgs() const {
            ConverterArgs ret;
            ret.add(ConverterArg::sLowFirstArgName, ConverterArgType::BOOL, false);
            //ret.add(ConverterArg::sSwapBytesArgName, ConverterArgType::BOOL, false);
            return ret;
        }

        virtual void setArgValues(const ConverterArgValues& args) {
            //setSwapBytes(args);
            bool low_first = DoubleRegisterArgTools::getLowFirst(args);
            if (low_first) {
                mLowByte = 0;
                mHighByte = 1;
            }
        };

        virtual ~UInt32Converter() {}
    private:
        int8_t mLowByte = 1;
        int8_t mHighByte = 0;
};
