#pragma once

#include "libmodmqttconv/converter.hpp"

class StringConverter : public DataConverter {
    public:
        virtual MqttValue toMqtt(const ModbusRegisters& data) const {
            const size_t size = data.getCount() * sizeof(std::uint16_t);
            uint8_t bytes[size];
            std::memcpy(&bytes, data.values().data(), size);
            ensureByteOrder(bytes, size);
            return MqttValue::fromBinary(bytes, size);
        }

        virtual ModbusRegisters toModbus(const MqttValue& value, int registerCount) const {
            ModbusRegisters ret;
            const size_t size = value.getBinarySize();
            const uint8_t* bytes = static_cast<const uint8_t*>(value.getBinaryPtr());
            for (size_t i = 0; (i < size - 1) && (ret.getCount() < registerCount); i += 2) {
                ret.appendValue(bytes[i + mLowByte] | (bytes[i + mHighByte] << 8));
            }
            return ret;
        }

        virtual void setArgs(const std::vector<std::string>& args) {
            if (args.size() == 0)
                return;
            std::string first_byte = ConverterTools::getArg(0, args);
            if (first_byte == "low_first") {
                mLowByte  = 0;
                mHighByte = 1;
            }
        }

        virtual ~StringConverter() {}
    private:
        int8_t mLowByte = 1;
        int8_t mHighByte = 0;

        void ensureByteOrder(uint8_t* bytes, const size_t size) const {
            if (mLowByte == 1) {
                for (size_t i = 0; i < size - 1; i += 2) {
                    const uint8_t c = bytes[i + 1];
                    bytes[i+1] = bytes[i];
                    bytes[i] = c;
                }
            }
        }
};
