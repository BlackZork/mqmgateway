#pragma once

#include "libmodmqttconv/convargs.hpp"
#include <memory>
#include <string>
#include <sys/types.h>
#include <vector>
#include <map>

namespace modmqttd {

class ConverterSpecification {
    public:
        static const ConverterArgValue sInvalidValue;

        std::string plugin;
        std::string converter;
        std::string arguments;
};

class ConverterNameParser {
    public:
        static ConverterSpecification parse(const std::string& spec);
        static ConverterArgValues parseArgs(const ConverterArgs& args, const std::string& arguments);
    private:
        static constexpr char const* RE_CONV = "([a-z0-9]+)\\.([a-z0-9]+)\\s?\\((.*)\\)";
};

} //namespace
