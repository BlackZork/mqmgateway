#pragma once

#include <cmath>
#include "libmodmqttconv/converter.hpp"

class Int32Converter : public DataConverter {
    public:
        virtual MqttValue toMqtt(const ModbusRegisters& data) const {
            int32_t val = ConverterTools::registersToInt32(data.values(), mLowFirst, mByteSwap);
            return MqttValue::fromInt(val);
        }

        virtual ModbusRegisters toModbus(const MqttValue& value, int registerCount) const {
            return ConverterTools::int32ToRegisters(value.getInt(), mLowFirst, mByteSwap, registerCount);
        }

        virtual void setArgs(const std::vector<std::string>& args) {
            switch (args.size()) {
                case 2: {
                    std::string swapBytes = ConverterTools::getArg(1, args);
                    mByteSwap = (swapBytes == "swap_bytes");
                }
                case 1: {
                    std::string firstByte = ConverterTools::getArg(0, args);
                    mLowFirst = (firstByte == "low_first");
                }
            }
        }

        virtual ~Int32Converter() {}
    private:
        bool mLowFirst = false;
        bool mByteSwap = false;
};
