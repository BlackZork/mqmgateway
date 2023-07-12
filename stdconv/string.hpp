#pragma once

#include "libmodmqttconv/converter.hpp"
#include <boost/algorithm/string/predicate.hpp>

class StringConverter : public DataConverter {
    public:
        virtual MqttValue toMqtt(const ModbusRegisters& data) const {
            std::vector<uint16_t> registers = data.values();
            ensureByteOrder(registers);
            return MqttValue::fromBinary(registers.data(), data.getCount() * sizeof(std::uint16_t));
        }

        virtual ModbusRegisters toModbus(const MqttValue& value, int registerCount) const {
            const uint16_t* source = static_cast<const uint16_t*>(value.getBinaryPtr());
            const int elementCount = value.getBinarySize() / sizeof(std::uint16_t);
            std::vector<uint16_t> registers(source, source + elementCount);
            ensureByteOrder(registers);
            registers.resize(std::min(elementCount, registerCount));
            return ModbusRegisters(registers);
        }

        virtual void setArgs(const std::vector<std::string>& args) {
            if (args.size() == 0) {
                return;
            }
            std::string encoding = ConverterTools::getArg(0, args);
            if (boost::algorithm::ends_with(encoding, "-be")) {
                mSwapByteOrder = 1;
            }
        }

        virtual ~StringConverter() {}
    private:
        int8_t mSwapByteOrder = 0;

        void ensureByteOrder(std::vector<uint16_t>& elements) const {
            if (mSwapByteOrder == 1) {
                ConverterTools::swapByteOrder(elements);
            }
        }
};
