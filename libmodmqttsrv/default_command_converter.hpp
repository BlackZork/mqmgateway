#pragma once

#include "libmodmqttconv/converter.hpp"

class ModbusRegisters;
class MqttValue;

namespace modmqttd {

class DefaultCommandConverter : public DataConverter {
    public:
        virtual ModbusRegisters toModbus(const MqttValue& value, int registerCount) const;
        static void parseAsJson(ModbusRegisters& ret, const std::string jsonData, int registerCount);
    private:
        static uint16_t to_uint16(int val);
};

}
