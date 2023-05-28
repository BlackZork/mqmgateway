#pragma once

#include <cmath>
#include "libmodmqttconv/converter.hpp"

class Int32Converter : public DataConverter {
    public:
        virtual MqttValue toMqtt(const ModbusRegisters& data) const {
            int high = 0, low = 0;
            if (data.getCount() > 1) {
                high = mHighByte;
                low = mLowByte;
            }
            int32_t val = data.getValue(high);
            if (data.getCount() > 1) {
                val = val << 16;
                val += data.getValue(low);
            }
            return MqttValue::fromInt(val);
        }

        virtual ModbusRegisters toModbus(const MqttValue& value, int registerCount) const {
            ModbusRegisters ret;
            int32_t val = value.getInt();
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
            if (args.size() == 0)
                return;
            std::string first_byte = ConverterTools::getArg(0, args);
            if (first_byte == "low_first") {
                mLowByte = 0;
                mHighByte = 1;
            }
        }

        virtual ~Int32Converter() {}
    private:
        int8_t mLowByte = 1;
        int8_t mHighByte = 0;
};
