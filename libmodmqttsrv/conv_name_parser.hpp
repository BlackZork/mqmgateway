#pragma once

#include <string>
#include <vector>

namespace modmqttd {

struct ConverterSpecification {
    std::string plugin;
    std::string converter;
    std::vector<std::string> args;
};

class ConverterNameParser {
    public:
        static ConverterSpecification parse(const std::string& spec);
    private:
        typedef enum {
            SCAN,
            STRING,
            ESCAPE
        } aState;

        static constexpr char const* RE_CONV = "([a-z0-9]+)\\.([a-z0-9]+)\\s?\\((.*)\\)";
        static std::vector<std::string> parseArgs(const std::string& argSpec);
};

} //namespace
