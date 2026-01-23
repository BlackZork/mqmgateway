#pragma once

#include <algorithm>
#include <map>
#include <stdexcept>
#include <vector>
#include <string>

#include "convtools.hpp"

using namespace std::string_literals;

enum ConverterArgType {
    INVALID = 0,
    INT = 1,
    STRING = 2
};

class ConverterArg {
    public:
        ConverterArg(const std::string& argName, ConverterArgType argType)
            : mName(argName), mArgType((argType))
        {}

        std::string mName;
        ConverterArgType mArgType;
};

class ConverterArgs : public std::vector<ConverterArg> {
    public:
        void add(const std::string& argName, ConverterArgType argType) {
            this->push_back(ConverterArg(argName, argType));
        }
};


class ConverterArgValue {
    public:
        ConverterArgType mArgType = ConverterArgType::INVALID;

        ConverterArgValue(ConverterArgType argType)
            : mArgType(argType)
        {}

        void setValue(const std::string& value) { mValue = value; }

        std::string as_str() const {return mValue; };
        int as_int() const { return ConverterTools::toInt(mValue); }
        double as_double() const { return ConverterTools::toDouble(mValue); }

        uint16_t as_uint16() const {
            int ret = ConverterTools::toInt(mValue, 16);
            if (ret < 0 || ret > 0xffff)
                throw std::out_of_range("value out of range");
            return (uint16_t)ret;
        }
    private:
        std::string mValue;
};

class ConverterArgValues {
    public:
        ConverterArgValues(const ConverterArgs& args) {
            for(auto it = args.begin(); it != args.end(); it++) {
                mValues.insert({it->mName, ConverterArgValue(it->mArgType)});
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

        void setArgValue(const std::string& name, ConverterArgType argType, const std::string& value) {
            auto it = mValues.find(name);
            if (it == mValues.end())
                throw std::invalid_argument("Wrong parameter name "s + name);
            it->second.setValue(value);
        }

        int count() const { return mValues.size(); }

        ConverterArgValue operator[](const std::string& argName) const {
            const ConverterArgValue val(getArgValue(argName));
            return val;
        }
    private:
        std::map<std::string, ConverterArgValue> mValues;
};
