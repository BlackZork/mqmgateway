#pragma once

#include "converter.hpp"
#include <string>

class ConverterPlugin {
    public:
        virtual std::string getName() const = 0;

        virtual DataConverter* getConverter(const std::string& name) = 0;

        virtual ~ConverterPlugin() {
        };
};
