#pragma once

#include "libmodmqttconv/converter.hpp"

class StringConverter : public DataConverter {
    public:
        virtual MqttValue toMqtt(const ModbusRegisters& data) const {
            std::vector<uint16_t> registers = data.values();
            ConverterTools::adaptToNetworkByteOrder(registers);
            const size_t size = findActualSize(registers);
            return MqttValue::fromBinary(registers.data(), size);
        }

        virtual ModbusRegisters toModbus(const MqttValue& value, int registerCount) const {
            std::vector<uint16_t> registers(registerCount, 0);
            std::memcpy(registers.data(), value.getBinaryPtr(), value.getBinarySize());
            ConverterTools::adaptToNetworkByteOrder(registers);
            return ModbusRegisters(registers);
        }

        virtual ~StringConverter() {}

    private:
        size_t findActualSize(const std::vector<uint16_t> registers) const {
            const size_t byteCount = registers.size() * sizeof(uint16_t);
            const uint8_t* firstByte = reinterpret_cast<const uint8_t*>(registers.data());
            for (size_t offset = 0; offset < byteCount; offset++) {
              if (*(firstByte + offset) == 0) {
                return offset;
              }
            }
            return byteCount;
        }
};
