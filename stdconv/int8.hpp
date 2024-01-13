#pragma once

#include <cmath>
#include "libmodmqttconv/converter.hpp"

class Int8Converter : public DataConverter {
    public:
        virtual MqttValue toMqtt(const ModbusRegisters& data) const {
            uint16_t val = data.getValue(0);
            if (mFirst) {
                val = val >> 8;
            }
            return MqttValue::fromInt((int8_t)val);
        }

        virtual void setArgs(const std::vector<std::string>& args) {
            if (args.size() == 0)
                return;
            std::string first_byte = ConverterTools::getArg(0, args);
            mFirst = (first_byte == "first");
        }
    private:
        bool mFirst = false;
};


class UInt8Converter : public DataConverter {
    public:
        virtual MqttValue toMqtt(const ModbusRegisters& data) const {
            uint16_t val = data.getValue(0);
            if (mFirst) {
                val = val >> 8;
            }
            return MqttValue::fromInt((uint8_t)val);
        }

        virtual void setArgs(const std::vector<std::string>& args) {
            if (args.size() == 0)
                return;
            std::string first_byte = ConverterTools::getArg(0, args);
            mFirst = (first_byte == "first");
        }
    private:
        bool mFirst = false;
};
