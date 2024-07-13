#pragma once

#include <map>
#include "int16.hpp"
#include "libmodmqttconv/converter.hpp"

class MapConverter : public DataConverter {
    public:
        typedef union {
            char* mStr;
            int mInt;
        } MqttVariant;

        virtual MqttValue toMqtt(const ModbusRegisters& data) const {
            // TODO should be detected at config parse time, not runtime
            // we need DataConverter::isSingleRegisterOnly() for this.
            if (data.getCount() != 1)
                throw ConvException("Cannot map multiple registers");

            std::map<uint16_t, MqttVariant>::const_iterator it = mMap.find(data.getValue(0));
            if (it == mMap.end()) {
                return MqttValue::fromInt(data.getValue(0));
            }

            if (mIsIntMap)
                return MqttValue::fromInt(it->second.mInt);
            else
                return MqttValue::fromString(it->second.mStr);
        }

        virtual ModbusRegisters toModbus(const MqttValue& value, int registerCount) const {
            if (registerCount != 1)
                throw ConvException("Cannot map multiple registers");

            std::map<uint16_t, MqttVariant>::const_iterator it = findVariant(value);
            if (it == mMap.end())
                return Int16Converter().toModbus(value, registerCount);

            return it->first;
        }

        virtual void setArgs(const std::vector<std::string>& args) {
            if (args.size() != 1)
                throw ConvException("This converter accepts a single argument only");

            parseMap(args[0]);
        }

        virtual ~MapConverter() {
            if (!mIsIntMap)
                for(auto& it: mMap) {
                    delete it.second.mStr;
                }
        }
    private:
        // register value to mqtt value
        std::map<uint16_t, MqttVariant> mMap;
        bool mIsIntMap = false;

        void parseMap(const std::string& pStrMap);
        std::map<uint16_t, MqttVariant>::const_iterator findVariant(const MqttValue& pValue) const;
};
