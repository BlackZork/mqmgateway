#pragma once

#include <algorithm>
#include <stack>
#include "int16.hpp"
#include "libmodmqttconv/converter.hpp"

class MapConverter : public DataConverter {
    friend class MapParser;
    public:
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

        virtual ConverterArgs getArgs() const {
            ConverterArgs ret;
            ret.add("map", ConverterArgType::STRING, "");
            return ret;
        }

        virtual void setArgValues(const ConverterArgValues& values) {
            const ConverterArgValue& val = values["map"];
            parseMap(val.as_str());
        }

        virtual ~MapConverter() {
        }
    private:
        typedef union {
            char* mStr;
            int mInt;
        } MqttVariant;
        /**
         * Do not use outside Map
         * destructor does not free memory allocated
         * for string variant
         */
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

                Mapping(const Mapping& pOther) {
                    *this = pOther;
                }

                Mapping& operator=(const Mapping& pOther) {
                    mRegisterValue = pOther.mRegisterValue;
                    mIsInt = pOther.isInt();
                    if (mIsInt)
                        mMqttValue.mInt = pOther.mMqttValue.mInt;
                    else
                        mMqttValue.mStr = pOther.mMqttValue.mStr;
                    return *this;
                }

                void dispose() {
                    if (!mIsInt && mMqttValue.mStr != nullptr)
                        delete mMqttValue.mStr;
                }

                ~Mapping() {}
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

                ~Map() {
                    for(auto& val: *this) {
                        val.dispose();
                    }
                }
        };

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

        typedef enum {
            NONE,
            INT,
            STRING
        } aValueType;

        std::stack<aState> mCurrentState;
        std::string mKey;

        aValueType mValueType;
        std::string mValue;

        void addEscapedChar(char c);
        void addMapping(MapConverter::Map& mappings);
};

