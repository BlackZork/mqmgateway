#include "conv_name_parser.hpp"

#include <exception>
#include <memory>
#include <regex>
#include <string>

#include "exceptions.hpp"
#include "libmodmqttconv/convargs.hpp"

using namespace std::string_literals;

namespace modmqttd {

class ConverterArgParser {
    public:
        ConverterArgParser(const ConverterArgs& args) : mArgs(args)
        {}

        ConverterArgValues parse(const std::string& argStr);
    private:
        typedef enum {
            SCAN,
            ARGVALUE,
            ESCAPE
        } aState;

        ConverterArgs mArgs;
        bool mUseArgOrder = true;
        std::stack<aState> mCurrentState;
        std::string argname;
        std::string argvalue;
        ConverterArgs::const_iterator it;

        void setArgValue(ConverterArgValues& values, const std::string& argName, ConverterArgType argType, const std::string& argValue);
        char getEscapedChar(const char& c) { return c; };
        const std::string& validateArgName(const std::string& argName);
};

const std::string&
ConverterArgParser::validateArgName(const std::string& argName) {
    //TODO check for chars outside a-zA-Z0-9
    return argName;
};

void
ConverterArgParser::setArgValue(ConverterArgValues& values, const std::string& argName, ConverterArgType argType, const std::string& argValue) {
    try {
        if (!argname.empty()) {
            mUseArgOrder = false;
        } else {
            if (!mUseArgOrder)
                throw ConvNameParserException("Cannot use positional argument after named argument "s + std::to_string(mArgs.size()));

            if (it == mArgs.end())
                throw ConvNameParserException("Too many arguments provided, need "s + std::to_string(mArgs.size()));
            argname = it->mName;
            it++;
        }

        values.setArgValue(argname, it->mArgType, argvalue);
        argname.clear();
        argvalue.clear();
    } catch (const std::exception& ex) {
        throw ConvNameParserException("Error setting argument " + argname + ":" + ex.what());
    }
};

ConverterArgValues
ConverterArgParser::parse(const std::string& argStr) {
    ConverterArgValues ret(mArgs);

    it = mArgs.begin();

    mCurrentState.push(SCAN);

    char str_delimiter = 0x0;

    for(const char& c: argStr) {
        switch(c) {
            case '\\':
                switch(mCurrentState.top()) {
                    case SCAN: mCurrentState.push(ESCAPE); break;
                    case ARGVALUE: argvalue += c; break;
                    case ESCAPE:
                        argvalue += getEscapedChar(c);
                        mCurrentState.pop();
                    break;
                }
            break;
            case '=':
                switch(mCurrentState.top()) {
                    case SCAN:
                        if (argvalue.empty())
                            throw ConvNameParserException("Missing name for argument " + std::to_string(ret.count() + 1));
                        if (!argname.empty())
                            throw ConvNameParserException("Name for argument " + std::to_string(ret.count() + 1) + " already set to " + argname);
                        argname = validateArgName(argvalue);
                        argvalue.clear();
                    break;
                    case ARGVALUE:
                        throw ConvNameParserException("Name for argument " + std::to_string(ret.count() + 1) + " cannot be quoted");
                    break;
                    case ESCAPE:
                        argvalue += getEscapedChar(c);
                        mCurrentState.pop();
                    break;
                }
            break;
            case ',':
                switch(mCurrentState.top()) {
                    case SCAN:
                        if (argvalue.empty())
                            throw ConvNameParserException("Argument " + std::to_string(ret.count() + 1) + " is empty");
                        setArgValue(ret, argname, it->mArgType, argvalue);
                    break;
                    case ARGVALUE: argvalue += c; break;
                    case ESCAPE:
                        argvalue += getEscapedChar(c);
                        mCurrentState.pop();
                    break;
                }
            break;
            case '"':
                switch(mCurrentState.top()) {
                    case SCAN:
                        mCurrentState.push(ARGVALUE);
                        str_delimiter = c;
                    break;
                    case ARGVALUE:
                        if (str_delimiter == c)
                            mCurrentState.pop();
                        else
                            argvalue += c;
                    break;
                    case ESCAPE:
                        argvalue += getEscapedChar(c);
                        mCurrentState.pop();
                    break;
                }
            break;
            case '\'':
                switch(mCurrentState.top()) {
                    case SCAN:
                        mCurrentState.push(ARGVALUE);
                        str_delimiter = c;
                    break;
                    case ARGVALUE:
                        if (str_delimiter == c)
                            mCurrentState.pop();
                        else
                            argvalue += c;
                        break;
                    case ESCAPE:
                        argvalue += getEscapedChar(c);
                        mCurrentState.pop();
                    break;
                }
            break;
            case ' ':
                switch(mCurrentState.top()) {
                    case SCAN: break;
                    case ARGVALUE: argvalue += c; break;
                    case ESCAPE: argvalue += getEscapedChar(c); mCurrentState.pop(); break;
                }
            break;
            default:
                switch(mCurrentState.top()) {
                    case SCAN:
                        argvalue += c;
                        break;
                    case ARGVALUE: argvalue += c; break;
                    case ESCAPE: argvalue += getEscapedChar(c); mCurrentState.pop(); break;
                }

            break;
        };
    }

    switch(mCurrentState.top()) {
        case SCAN:
            if (argvalue.size())
                setArgValue(ret, argname, it->mArgType, argvalue);
            break;
        case ARGVALUE:
            throw ConvNameParserException("Argument "s + std::to_string(ret.count()) + " is an unterminated string");
        case ESCAPE:
            throw ConvNameParserException("Argument "s + std::to_string(ret.count()) + " has an invalid escape sequence");
    }

    return ret;
}

ConverterSpecification
ConverterNameParser::parse(const std::string& spec) {
    std::regex re(RE_CONV);

    std::cmatch matches;
    if (!std::regex_match(spec.c_str(), matches, re))
        throw ConvNameParserException("Supply converter spec in form: plugin.converter(value1, param=value2, …)");

    ConverterSpecification ret;

    ret.plugin = matches[1];
    ret.converter = matches[2];

    ret.arguments = matches[3];
    return ret;
}

ConverterArgValues
ConverterNameParser::parseArgs(const ConverterArgs& args, const std::string& arguments) {
    ConverterArgParser p(args);
    return p.parse(arguments);
}

}; //namespace
