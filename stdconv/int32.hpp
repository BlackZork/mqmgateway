#pragma once

#include <cmath>
#include "libmodmqttconv/converter.hpp"

class Int32Converter : public DataConverter {
    public:
        virtual MqttValue toMqtt(const ModbusRegisters& data) const {
            int val = data.getValue(0);
            if (data.getCount() > 1) {
                val = val << 16;
                val += data.getValue(1);
            }
            return MqttValue::fromInt(val);
        }

        virtual ModbusRegisters toModbus(const MqttValue& value, int registerCount) const {
            ModbusRegisters ret;
            int val = value.getInt();
            ret.appendValue(val);
            if (registerCount == 2) {
                ret.prependValue(val >> 16);
            }
            return ret;
        }


        virtual ~Int32Converter() {}
    private:
};
