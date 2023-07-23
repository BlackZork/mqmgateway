#pragma once

#include "libmodmqttconv/converter.hpp"
#include <map>

class StringConverter : public DataConverter {
    public:

        /**
         * Supported encodings
         */
        typedef enum {
            NONE       = 1,
            ASCII16_BE = 2,
        } Encoding;

        virtual MqttValue toMqtt(const ModbusRegisters& data) const {
            std::vector<uint16_t> registers = data.values();
            applyEncoding(registers);
            return MqttValue::fromBinary(registers.data(), data.getCount() * sizeof(std::uint16_t));
        }

        virtual ModbusRegisters toModbus(const MqttValue& value, int registerCount) const {
            const uint16_t* source = static_cast<const uint16_t*>(value.getBinaryPtr());
            const int elementCount = value.getBinarySize() / sizeof(std::uint16_t);
            std::vector<uint16_t> registers(source, source + elementCount);
            applyEncoding(registers);
            registers.resize(std::min(elementCount, registerCount));
            return ModbusRegisters(registers);
        }

        virtual void setArgs(const std::vector<std::string>& args) {
            if (args.size() == 0) {
                return;
            }
            std::string encodingName = ConverterTools::getArg(0, args);
            if (mEncodings.count(encodingName) == 0) {
                throw ConvException("Unsupported encoding: " + encodingName);
            }
            mEncoding = mEncodings.at(encodingName);
        }

        virtual ~StringConverter() {}

    private:
        Encoding mEncoding = NONE;
        const std::map<std::string, Encoding> mEncodings = {
            {"none", NONE},
            {"ascii16-be", ASCII16_BE},
        };

        void applyEncoding(std::vector<uint16_t>& elements) const {
            if (mEncoding == ASCII16_BE) {
                ConverterTools::adaptToNetworkByteOrder(elements);
            }
        }
};
