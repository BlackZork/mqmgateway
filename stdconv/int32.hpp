#pragma once

#include <cmath>
#include "libmodmqttconv/converter.hpp"

class Int32Converter : public IStateConverter {
    public:
        virtual MqttValue toMqtt(const ModbusRegisters& data) const {
            int val = data.getValue(0);
            if (data.getCount() > 1) {
                val = val << 16;
                val += data.getValue(1);
            }
            return MqttValue::fromInt(val);
        }

        virtual ~Int32Converter() {}
    private:
};
