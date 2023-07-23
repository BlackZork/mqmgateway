#pragma once

#include "libmodmqttconv/converter.hpp"

class StringConverter : public DataConverter {
    public:
        virtual MqttValue toMqtt(const ModbusRegisters& data) const {
            std::vector<uint16_t> registers = data.values();
            ConverterTools::adaptToNetworkByteOrder(registers);
            size_t size = std::min(mMaxSize, data.getCount() * sizeof(uint16_t));
            return MqttValue::fromBinary(registers.data(), size);
        }

        virtual ModbusRegisters toModbus(const MqttValue& value, int registerCount) const {
            const uint16_t* source = static_cast<const uint16_t*>(value.getBinaryPtr());
            const int valueCount = value.getBinarySize() / sizeof(uint16_t);
            std::vector<uint16_t> registers(source, source + valueCount);
            ConverterTools::adaptToNetworkByteOrder(registers);
            registers.resize(std::min(valueCount, registerCount));
            return ModbusRegisters(registers);
        }

        virtual void setArgs(const std::vector<std::string>& args) {
            if (args.size() == 0) {
                return;
            }
            mMaxSize = ConverterTools::getIntArg(0, args);
        }

        virtual ~StringConverter() {}

    private:
        size_t mMaxSize = std::numeric_limits<size_t>::max();
};
