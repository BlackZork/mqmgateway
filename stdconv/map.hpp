#pragma once

#include <algorithm>
#include <stack>
#include "int16.hpp"
#include "libmodmqttconv/converter.hpp"

class MapConverter : public DataConverter {
    public:
        typedef union {
            char* mStr;
            int mInt;
        } MqttVariant;

        class Mapping {
            public:
                Mapping(uint16_t registerValue, int mqttIntValue)
                    : mRegisterValue(registerValue),
                      mIsInt(true)
                {
                    mMqttValue.mInt = mqttIntValue;
                }
                Mapping(uint16_t registerValue, const std::string& mqttStrValue)
                    : mRegisterValue(registerValue),
                      mIsInt(false)
                {
                    mMqttValue.mStr = new char[mqttStrValue.size() + 1];
                    strcpy(mMqttValue.mStr, mqttStrValue.c_str());
                }
                Mapping() : Mapping(0,0) {};

                ~Mapping() {
                    if (!mIsInt && mMqttValue.mStr != nullptr)
                        delete mMqttValue.mStr;
                }
                uint16_t mRegisterValue;

                bool isInt() const { return mIsInt; }
                const char* getStrMqttValue() const { return mMqttValue.mStr; }
                int getIntMqttValue() const { return mMqttValue.mInt; }
            private:
                bool mIsInt;
                MqttVariant mMqttValue;
        };

        class Map : public std::vector<Mapping> {
            public:
                std::vector<Mapping>::const_iterator findVariant(const MqttValue& pValue) const;
                std::vector<Mapping>::const_iterator findRegValue(uint16_t value) const {
                    return std::find_if(
                        begin(),
                        end(),
                        [&value](const Mapping& mapping) -> bool { return mapping.mRegisterValue == value; }
                    );
                }
        };

        virtual MqttValue toMqtt(const ModbusRegisters& data) const {
            // TODO should be detected at config parse time, not runtime
            // we need DataConverter::isSingleRegisterOnly() for this.
            if (data.getCount() != 1)
                throw ConvException("Cannot map multiple registers");

            std::vector<Mapping>::const_iterator it = mMappings.findRegValue(data.getValue(0));
            if (it == mMappings.end()) {
                return MqttValue::fromInt(data.getValue(0));
            }

            if (it->isInt())
                return MqttValue::fromInt(it->getIntMqttValue());
            else
                return MqttValue::fromString(it->getStrMqttValue());
        }

        virtual ModbusRegisters toModbus(const MqttValue& value, int registerCount) const {
            if (registerCount != 1)
                throw ConvException("Cannot map multiple registers");

            std::vector<Mapping>::const_iterator it = mMappings.findVariant(value);
            if (it == mMappings.end())
                return Int16Converter().toModbus(value, registerCount);

            return it->mRegisterValue;
        }

        virtual void setArgs(const std::vector<std::string>& args) {
            if (args.size() != 1)
                throw ConvException("This converter accepts a single argument only");

            parseMap(args[0]);
        }

        virtual ~MapConverter() {
        }
    private:
        // register value to mqtt value
        Map mMappings;
        void parseMap(const std::string& pStrMap);
};

class MapParser {
    public:
        void parse(MapConverter::Map& mappings, const std::string& data);
    private:
        typedef enum {
            SCAN,
            KEY,
            ESCAPE,
            VALUE
        } aState;

        std::stack<aState> mCurrentState;
        std::string mKey;
        bool mIsIntKey;
        std::string mValue;

        void addEscapedChar(char c);
        void addMapping(MapConverter::Map& mappings);
};

