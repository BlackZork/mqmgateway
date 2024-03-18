#pragma once

#include <cmath>
#include "libmodmqttconv/converter.hpp"

class BitmaskConverter : public DataConverter {
    public:
        virtual MqttValue toMqtt(const ModbusRegisters& data) const {
            int val = data.getValue(0) & mask;
            return MqttValue::fromInt(val);
        }

        virtual void setArgs(const std::vector<std::string>& args) {
            mask = ConverterTools::getHex16Arg(0, args);
        }

        virtual ~BitmaskConverter() {}
    private:
        uint16_t mask = 0xffff;
};


class BitConverter : public DataConverter {
    public:
        virtual MqttValue toMqtt(const ModbusRegisters& data) const {
            int val = data.getValue(0) >> (bitNumber-1) & 0x1;
            return MqttValue::fromInt(val);
        }

        virtual void setArgs(const std::vector<std::string>& args) {
            int number = ConverterTools::getIntArg(0, args);
            if (16 < number || number < 0)
                throw ConvException("Please provide a valid bit number [1-16]");
            bitNumber = number;
        }

        virtual ~BitConverter() {}
    private:
        uint8_t bitNumber = -1;
};
