#pragma once

#include <boost/config.hpp> // for BOOST_SYMBOL_EXPORT

#include "libmodmqttconv/converterplugin.hpp"

class StdConvPlugin : ConverterPlugin {
    public:
        virtual std::string getName() const { return "expr"; }
        virtual DataConverter* getConverter(const std::string& name);
        virtual ~StdConvPlugin() {}
};

extern "C" BOOST_SYMBOL_EXPORT StdConvPlugin converter_plugin;
StdConvPlugin converter_plugin;
