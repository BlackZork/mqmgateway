#include "default_command_converter.hpp"
#include "libmodmqttconv/modbusregisters.hpp"
#include "libmodmqttconv/convexception.hpp"


namespace modmqttd {

ModbusRegisters
DefaultCommandConverter::toModbus(const MqttValue& value, int registerCount) const {
    ModbusRegisters ret;

    if (registerCount > 1) {
        parseAsJson(ret, value.getString(), registerCount);
    } else {
        try {
            int32_t temp = value.getInt();
            if (temp <= static_cast<int>(UINT16_MAX) && temp >=0) {
                uint16_t regval = static_cast<uint16_t>(temp);
                ret.addValue(regval);
            } else {
                throw ConvException(std::string("Conversion failed, value " + std::to_string(temp) + " out of range"));
            }
        } catch (const std::invalid_argument& ex) {
            throw ConvException("Failed to convert mqtt value to int16");
        } catch (const std::out_of_range& ex) {
            throw ConvException("mqtt value is out of range");
        }
    }
    return ret;
}

void
DefaultCommandConverter::parseAsJson(ModbusRegisters& ret, const std::string jsonData, int registerCount) {
    throw ConvException("Not implemented yet");
}

}
