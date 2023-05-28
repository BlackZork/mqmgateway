#pragma once

#include <cmath>
#include "libmodmqttconv/converter.hpp"

class UInt32Converter : public DataConverter {
    public:
        virtual MqttValue toMqtt(const ModbusRegisters& data) const {
            uint32_t val = data.getValue(mHighByte);
            if (data.getCount() > 1) {
                val = val << 16;
                val += data.getValue(mLowByte);
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

        virtual void setArgs(const std::vector<std::string>& args) {
            std::string first_byte = ConverterTools::getArg(0, args);
            if (first_byte == "low_first") {
                mLowByte = 0;
                mHighByte = 1;
            }
        }

        virtual ~UInt32Converter() {}
    private:
        int8_t mLowByte = 1;
        int8_t mHighByte = 0;
};
