#pragma once

#include "libmodmqttconv/converter.hpp"

/**
 * This converter supports string data read from modbus
 * device in uint16 registers. We assume that string data layout is
 * byte-after-byte in modbus packet.
 *
 * On low-endian host register bytes are swapped, so we need to
 * swap them back before feeding string with subsequent bytes from register data.
 *
 * Data is iterpreted as C-style string. Reading ends when 0x0 byte is found.
 *
 * When writing, all bytes from mqtt payload are written, no matter if they contain 0x0
 * in the middle or not. If payload size is shorter, then remaining register bytes are
 * filled with 0x0 byte values.
 */
class StringConverter : public DataConverter {
    public:
        virtual MqttValue toMqtt(const ModbusRegisters& data) const {
            std::vector<uint16_t> registers = data.values();
            ConverterTools::adaptToNetworkByteOrder(registers);
            const size_t size = findCStringSize(registers);
            return MqttValue(registers.data(), size);
        }

        virtual ModbusRegisters toModbus(const MqttValue& value, int registerCount) const {
            std::vector<uint16_t> registers(registerCount, 0);
            const char* strPtr = (const char*)value.getBinaryPtr();
            int strLen = value.getBinarySize();
            // copy string bytes to registers
            // fill remaining space with 0x0 bytes
            for(int i = 0; i < registerCount; i++) {
                if (strLen == 0) {
                    registers[i] = 0;
                } else if (strLen == 1) {
                    uint16_t val = (uint16_t)*strPtr;
                    registers[i] =  val << 8;
                    strLen--;
                } else {
                    uint16_t val = (uint16_t)*strPtr;
                    strLen -= 2;
                    registers[i] = val << 8;
                    strPtr++;
                    val = (uint16_t)*strPtr;
                    registers[i] += val;
                    strPtr++;
                }
            }
            ConverterTools::adaptToNetworkByteOrder(registers);
            return ModbusRegisters(registers);
        }

        virtual ~StringConverter() {}

    private:
        size_t findCStringSize(const std::vector<uint16_t> registers) const {
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
