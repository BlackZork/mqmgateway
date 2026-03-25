#pragma once

#include <cmath>
#include "libmodmqttconv/converter.hpp"
#include "libmodmqttconv/convtools.hpp"
#include "argtools.hpp"

class Int16Converter : public DataConverter {
    public:
        virtual ConverterArgs getArgs() const {
            ConverterArgs ret;
            ret.add(ConverterArg::sSwapBytesArgName, ConverterArgType::BOOL, false);
            return ret;
        }

        virtual void setArgValues(const ConverterArgValues& args) {
            mSwapBytes = DoubleRegisterArgTools::getSwapBytes(args);
        };

        virtual MqttValue toMqtt(const ModbusRegisters& data) const {
            uint16_t val = data.getValue(0);
            val = ConverterTools::setByteOrder(val, mSwapBytes);
            return MqttValue::fromInt((int16_t)val);
        }

        virtual ModbusRegisters toModbus(const MqttValue& value, int registerCount) const {
            ModbusRegisters ret;
            int32_t val = value.getInt();
            if (val < INT16_MIN || val > INT16_MAX)
                throw ConvException(std::string("Conversion failed, value " + std::to_string(val) + " out of range"));

            val = ConverterTools::setByteOrder(val, mSwapBytes);
            for (int i = 0; i < registerCount; i++) {
                ret.appendValue(val);
            }
            return ret;
        }
    private:
        bool mSwapBytes;
};

class UInt16Converter : public DataConverter {
    public:
        virtual ConverterArgs getArgs() const {
            ConverterArgs ret;
            ret.add(ConverterArg::sSwapBytesArgName, ConverterArgType::BOOL, false);
            return ret;
        }

        virtual void setArgValues(const ConverterArgValues& args) {
            mSwapBytes = DoubleRegisterArgTools::getSwapBytes(args);
        };

        virtual MqttValue toMqtt(const ModbusRegisters& data) const {
            uint16_t val = data.getValue(0);
            val = ConverterTools::setByteOrder(val, mSwapBytes);
            return MqttValue::fromInt(val);
        }

        virtual ModbusRegisters toModbus(const MqttValue& value, int registerCount) const {
            ModbusRegisters ret;
            int32_t val = value.getInt();
            if (val < 0 || val > UINT16_MAX)
                throw ConvException(std::string("Conversion failed, value " + std::to_string(val) + " out of range"));

            val = ConverterTools::setByteOrder(val, mSwapBytes);
            for (int i = 0; i < registerCount; i++) {
                ret.appendValue(val);
            }
            return ret;
        }
    private:
        bool mSwapBytes;
};

