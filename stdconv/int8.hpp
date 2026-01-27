#pragma once

#include <cmath>
#include "libmodmqttconv/converter.hpp"

class Int8Base : public DataConverter {
        virtual ConverterArgs getArgs() const {
            ConverterArgs ret;
            ret.add("first", ConverterArgType::BOOL, false);
            return ret;
        }

        virtual void setArgValues(const ConverterArgValues& values) {
            const ConverterArgValue& val = values["first"];
            try {
                mFirst = val.as_bool();
            } catch (const ConvException& ex) {
                std::string oldval = val.as_str();
                if (oldval == "")
                    mFirst = false;
                else if (oldval == "first")
                    mFirst = true;
                else
                    throw;
                std::cerr << "[WARN] first param changed to bool, please update to 'first=true'" << std::endl;
            }
        };

    protected:
        bool mFirst = false;
};

class Int8Converter : public Int8Base {
    public:
        virtual MqttValue toMqtt(const ModbusRegisters& data) const {
            uint16_t val = data.getValue(0);
            if (mFirst) {
                val = val >> 8;
            }
            return MqttValue::fromInt((int8_t)val);
        }
};


class UInt8Converter : public Int8Base {
    public:
        virtual MqttValue toMqtt(const ModbusRegisters& data) const {
            uint16_t val = data.getValue(0);
            if (mFirst) {
                val = val >> 8;
            }
            return MqttValue::fromInt((uint8_t)val);
        }
};
