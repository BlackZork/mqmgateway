#pragma once

#include <cmath>
#include "libmodmqttconv/converter.hpp"
#include "libmodmqttconv/convexception.hpp"

class FloatConverter : public DataConverter {
    public:
        virtual MqttValue toMqtt(const ModbusRegisters& data) const {
            if (data.getCount() < 2)
                throw ConvException("Cannot read 32-bit float from single register");
            std::vector<uint16_t> converted(data.values());
            if (mSwapBytes)
                ConverterTools::swapByteOrder(converted);
            int32_t int_val = ConverterTools::registersToInt32(converted, mLowFirst);
            double val = *reinterpret_cast<float*>(&int_val);
            return MqttValue::fromDouble(val, mPrecision);
        }

        virtual ModbusRegisters toModbus(const MqttValue& value, int registerCount) const {
            if (registerCount < 2)
                throw ConvException("Cannot store float in single register");

            float val = value.getDouble();

            std::vector<uint16_t> regdata(
                ConverterTools::int32ToRegisters(*reinterpret_cast<int*>(&val), mLowFirst, registerCount)
            );

            if (mSwapBytes)
                ConverterTools::swapByteOrder(regdata);
            return ModbusRegisters(regdata);
        }

        virtual void setArgs(const std::vector<std::string>& args) {
            switch (args.size()) {
                case 3: {
                    std::string swapBytes = ConverterTools::getArg(2, args);
                    mSwapBytes = (swapBytes == "swap_bytes");
                }
                case 2: {
                    std::string firstByte = ConverterTools::getArg(1, args);
                    mLowFirst = (firstByte == "low_first");
                }
                case 1: {
                    mPrecision = ConverterTools::getIntArg(0, args);
                }
            }
        }

        virtual ~FloatConverter() {}
    private:
        bool mLowFirst = false;
        bool mSwapBytes = false;
        int mPrecision = -1;
};
