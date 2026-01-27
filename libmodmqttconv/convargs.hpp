#pragma once

#include <algorithm>
#include <map>
#include <stdexcept>
#include <vector>
#include <string>

#include "convtools.hpp"
#include "libmodmqttconv/convexception.hpp"

using namespace std::string_literals;

enum ConverterArgType {
    INVALID = 0,
    INT = 1,
    STRING = 2,
    DOUBLE = 3,
    BOOL = 4
};

class ConverterArg {
    public:
        static constexpr const char* sPrecisionArgName = "precision";
        static constexpr const char* sSwapBytesArgName = "swap_bytes";
        static constexpr const char* sLowFirstArgName = "low_first";

        ConverterArg(const std::string& argName, ConverterArgType argType, const std::string& defvalue)
            : mName(argName), mArgType(argType), mDefaultValue(defvalue)
        {}
        ConverterArg(const std::string& argName, ConverterArgType argType, int defvalue)
            : mName(argName), mArgType(argType), mDefaultValue(std::to_string(defvalue))
        {}
        ConverterArg(const std::string& argName, ConverterArgType argType, double defvalue)
            : mName(argName), mArgType(argType), mDefaultValue(std::to_string(defvalue))
        {}

        const std::string& getDefaultValue() const { return mDefaultValue; }

        std::string mName;
        ConverterArgType mArgType;

    private:
        std::string mDefaultValue;
};

class ConverterArgs : public std::vector<ConverterArg> {
    public:
        void add(const std::string& argName, ConverterArgType argType, const std::string& defValue) {
            this->push_back(ConverterArg(argName, argType, defValue));
        }
        void add(const std::string& argName, ConverterArgType argType, int defValue) {
            this->push_back(ConverterArg(argName, argType, defValue));
        }
        void add(const std::string& argName, ConverterArgType argType, double defValue) {
            this->push_back(ConverterArg(argName, argType, defValue));
        }
};


class ConverterArgValue {
    private:
        std::string mArgName;
        std::string mValue;
    public:
        static constexpr int NO_PRECISION=-1;

        ConverterArgType mArgType;

        ConverterArgValue(const std::string& argName, ConverterArgType argType, const std::string& defValue)
            : mArgName(argName), mArgType(argType)
        {
            setValue(defValue);
        }

        void setValue(const std::string& value) { mValue = value; }

        std::string as_str() const {return mValue; };

        int as_int() const {
            try {
                int base = 10;
                if (mValue.rfind("0x") == 0)
                    base = 16;
                return ConverterTools::toInt(mValue, base);
            } catch (const std::exception& ex) {
                throw ConvException("Invalid"s + mArgName + " int value:" + ex.what());
            }
        }

        double as_double() const {
            try {
                return ConverterTools::toDouble(mValue);
            } catch (const std::exception& ex) {
                throw ConvException("Invalid"s + mArgName + " double value:" + ex.what());
            }
        }

        uint16_t as_uint16() const {
            try {
                int ret = ConverterTools::toInt(mValue, 16);
                if (ret < 0 || ret > 0xffff)
                    throw std::out_of_range("value out of range");
                return (uint16_t)ret;
            } catch (const std::exception& ex) {
                throw ConvException("Invalid"s + mArgName + " uint16 value:" + ex.what());
            }
        }

        bool as_bool() const {
            if (mValue == "true" || mValue == "TRUE" || mValue=="1")
                return true;
            else if (mValue == "false" || mValue == "FALSE" || mValue == "0")
                return false;
            throw ConvException(mArgName + " value cannot be converted to bool");
        }
};

class ConverterArgValues {
    public:
        ConverterArgValues(const ConverterArgs& args) {
            for(auto it = args.begin(); it != args.end(); it++) {
                mValues.insert({it->mName, ConverterArgValue(it->mName, it->mArgType, it->getDefaultValue())});
            }
        }

        const ConverterArgValue& getArgValue(const std::string& name) const {
            std::map<std::string, ConverterArgValue>::const_iterator it =
                mValues.find(name);
            if (it == mValues.end())
                throw std::invalid_argument("no value for "s + name);
            return it->second;
        }

        bool hasArgValue(const std::string& name) {
            std::map<std::string, ConverterArgValue>::const_iterator it =
                mValues.find(name);
            return it != mValues.end();
        }

        void setArgValue(const std::string& name, const std::string& value) {
            auto it = mValues.find(name);
            if (it == mValues.end())
                throw std::invalid_argument("Wrong parameter name "s + name);
            it->second.setValue(value);
        }

        int count() const { return mValues.size(); }

        const ConverterArgValue& operator[](const std::string& argName) const {
            const ConverterArgValue& val(getArgValue(argName));
            return val;
        }
    private:
        std::map<std::string, ConverterArgValue> mValues;
};
