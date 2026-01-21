#pragma once

#include <vector>
#include <string>


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
