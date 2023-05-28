#pragma once

#include <cmath>
#include "libmodmqttconv/converter.hpp"

class Int16Converter : public DataConverter {
    public:
        virtual MqttValue toMqtt(const ModbusRegisters& data) const {
            uint16_t val = data.getValue(0);
            return MqttValue::fromInt((int16_t)val);
        }
};
