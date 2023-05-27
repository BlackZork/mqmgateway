#include "default_command_converter.hpp"
#include "libmodmqttconv/modbusregisters.hpp"
#include "libmodmqttconv/convexception.hpp"

#include <rapidjson/document.h>

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
                ret.appendValue(regval);
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
    rapidjson::Document doc;
    doc.Parse(jsonData.c_str());

    if (!doc.IsArray())
        throw ConvException("Only json array is supported when converting to multiple registers");

    if (doc.Size() != registerCount) {
        throw ConvException(std::string("Wrong json array size (" + std::to_string(doc.Size()) + "), need " + std::to_string(registerCount)));
    }

    for(auto& jv: doc.GetArray()) {
        ret.appendValue(to_uint16(jv.GetInt()));
    }
}

uint16_t
DefaultCommandConverter::to_uint16(int val) {
    if (val <= static_cast<int>(UINT16_MAX) && val >=0) {
        uint16_t regval = static_cast<uint16_t>(val);
        return regval;
    }
    throw ConvException(std::string("Conversion failed, register value " + std::to_string(val) + " out of range"));
}

}
