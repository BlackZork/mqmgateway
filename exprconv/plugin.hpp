#pragma once

#include "libmodmqttconv/converterplugin.hpp"

class ExprConvPlugin : ConverterPlugin {
    public:
        virtual std::string getName() const { return "expr"; }
        virtual DataConverter* getConverter(const std::string& pName);
        virtual ~ExprConvPlugin() {}
};

extern "C" ExprConvPlugin converter_plugin;
ExprConvPlugin converter_plugin;
