#pragma once

#include <cmath>
#include "libmodmqttconv/converter.hpp"

class Int16Converter : public DataConverter {
    public:
        virtual MqttValue toMqtt(const ModbusRegisters& data) const {
            uint16_t val = data.getValue(0);
            return MqttValue::fromInt((int16_t)val);
        }

        virtual ModbusRegisters toModbus(const MqttValue& value, int registerCount) const {
            ModbusRegisters ret;
            int32_t val = value.getInt();
            if (val < INT16_MIN || val > INT16_MAX)
                throw ConvException(std::string("Conversion failed, value " + std::to_string(val) + " out of range"));

            for (int i = 0; i < registerCount; i++) {
                ret.appendValue(val);
            }
            return ret;
        }

};
