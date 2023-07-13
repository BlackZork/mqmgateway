#pragma once

#include <cmath>
#include "libmodmqttconv/converter.hpp"

class Int32Converter : public DataConverter {
    public:
        virtual MqttValue toMqtt(const ModbusRegisters& data) const {
            int32_t val = ConverterTools::registersToInt32(data, mLowFirst);
            return MqttValue::fromInt(val);
        }

        virtual ModbusRegisters toModbus(const MqttValue& value, int registerCount) const {
            return ConverterTools::int32ToRegisters(value.getInt(), mLowFirst, registerCount);
        }

        virtual void setArgs(const std::vector<std::string>& args) {
            if (args.size() == 0)
                return;
            std::string first_byte = ConverterTools::getArg(0, args);
            mLowFirst = (first_byte == "low_first");
        }

        virtual ~Int32Converter() {}
    private:
        bool mLowFirst = false;
};
