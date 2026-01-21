#pragma once

#include <map>
#include "libmodmqttconv/convargs.hpp"
#include "libmodmqttconv/converter.hpp"

class BitmaskConverter : public DataConverter {
    public:
        virtual MqttValue toMqtt(const ModbusRegisters& data) const {
            int val = data.getValue(0) & mask;
            return MqttValue::fromInt(val);
        }

        virtual ConverterArgs getArgs() const {
            ConverterArgs ret;
            ret.push_back(ConverterArg("mask", ConverterArgType::INT));
            return ret;
        };

        virtual void setArgValues(const ConverterArgValues& args) {
            mask = args["mask"].as_uint16();
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

        virtual ConverterArgs getArgs() const {
            ConverterArgs ret;
            ret.push_back(ConverterArg("bit", ConverterArgType::INT));
            return ret;
        };

        virtual void setArgValues(const ConverterArgValues& args) {
            int number = args["bit"].as_int();
            if (16 < number || number < 0)
                throw ConvException("Please provide a valid bit number [1-16]");
            bitNumber = number;
        }

        virtual ~BitConverter() {}
    private:
        uint8_t bitNumber = -1;
};
