#pragma once

#include "libmodmqttconv/convargs.hpp"
#include <memory>
#include <string>
#include <sys/types.h>
#include <vector>
#include <map>

namespace modmqttd {

class ConverterArgValue {
    public:
        ConverterArgType mArgType;

        ConverterArgValue(ConverterArgType argType, const std::string& value);

        std::string str() const {return std::string(mStrVal); };
    private:
        union {
            int mIntVal;
            int64_t mInt64Val;
            double mDoubleVal;
            const char* mStrVal;
        };
};

class ConverterSpecification {
    public:
        static const ConverterArgValue sInvalidValue;

        std::string plugin;
        std::string converter;
        void addArgValue(const std::string& name, ConverterArgType argType, const std::string& value) {
            args[name].reset(new ConverterArgValue(argType, value));
        }
        const ConverterArgValue& getArgValue(const std::string& name) const {
            std::map<std::string, std::shared_ptr<ConverterArgValue>>::const_iterator it =
                args.find(name);
            if (it == args.end())
                return sInvalidValue;
            return *(it->second);
        }

        int getArgCount() const { return args.size(); }

        std::string operator[](const std::string& argName) const {
            const ConverterArgValue val(getArgValue(argName));
            return val.str();
        }
    private:
        std::map<std::string, std::shared_ptr<ConverterArgValue>> args;
};

class ConverterNameParser {
    public:
        static ConverterSpecification parse(const ConverterArgs& args, const std::string& spec);
    private:
        typedef enum {
            SCAN,
            STRING,
            ESCAPE
        } aState;

        static constexpr char const* RE_CONV = "([a-z0-9]+)\\.([a-z0-9]+)\\s?\\((.*)\\)";
        static void parseArgs(ConverterSpecification& spec, const ConverterArgs& args, const std::string& argSpec);
};

} //namespace
