#include "conv_name_parser.hpp"

#include <memory>
#include <regex>
#include <string>

#include "exceptions.hpp"
#include "libmodmqttconv/convargs.hpp"

using namespace std::string_literals;

namespace modmqttd {

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

const std::string&
validateArgName(const std::string& argName) {
    //TODO check for chars outside a-zA-Z0-9
    return argName;
}

char
getEscapedChar(const char& c) {
    return c;
};

ConverterArgValues
ConverterNameParser::parseArgs(const ConverterArgs& args, const std::string& argSpec) {
    ConverterArgValues ret(args);

    ConverterArgs::const_iterator it = args.begin();

    bool use_arg_order = true;

    std::stack<aState> currentState;
    currentState.push(SCAN);

    std::string argname;
    std::string argvalue;
    char str_delimiter = 0x0;

    for(const char& c: argSpec) {
        switch(c) {
            case '\\':
                switch(currentState.top()) {
                    case SCAN: currentState.push(ESCAPE); break;
                    case ARGVALUE: argvalue += c; break;
                    case ESCAPE:
                        argvalue += getEscapedChar(c);
                        currentState.pop();
                    break;
                }
            break;
            case '=':
                switch(currentState.top()) {
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
                        currentState.pop();
                    break;
                }
            break;
            case ',':
                switch(currentState.top()) {
                    case SCAN:
                        if (argvalue.empty())
                            throw ConvNameParserException("Argument " + std::to_string(ret.count() + 1) + " is empty");

                        if (!argname.empty()) {
                            use_arg_order = false;
                        } else {
                            if (!use_arg_order)
                                throw ConvNameParserException("Cannot use positional argument after named argument "s + std::to_string(args.size()));

                            if (it == args.end())
                                throw ConvNameParserException("Too many arguments provided, need "s + std::to_string(args.size()));
                            argname = it->mName;
                            it++;
                        }

                        ret.setArgValue(argname, it->mArgType, argvalue);
                        argname.clear();
                        argvalue.clear();
                    break;
                    case ARGVALUE: argvalue += c; break;
                    case ESCAPE:
                        argvalue += getEscapedChar(c);
                        currentState.pop();
                    break;
                }
            break;
            case '"':
                switch(currentState.top()) {
                    case SCAN:
                        currentState.push(ARGVALUE);
                        str_delimiter = c;
                    break;
                    case ARGVALUE:
                        if (str_delimiter == c)
                            currentState.pop();
                        else
                            argvalue += c;
                    break;
                    case ESCAPE:
                        argvalue += getEscapedChar(c);
                        currentState.pop();
                    break;
                }
            break;
            case '\'':
                switch(currentState.top()) {
                    case SCAN:
                        currentState.push(ARGVALUE);
                        str_delimiter = c;
                    break;
                    case ARGVALUE:
                        if (str_delimiter == c)
                            currentState.pop();
                        else
                            argvalue += c;
                        break;
                    case ESCAPE:
                        argvalue += getEscapedChar(c);
                        currentState.pop();
                    break;
                }
            break;
            case ' ':
                switch(currentState.top()) {
                    case SCAN: break;
                    case ARGVALUE: argvalue += c; break;
                    case ESCAPE: argvalue += getEscapedChar(c); currentState.pop(); break;
                }
            break;
            default:
                switch(currentState.top()) {
                    case SCAN:
                        argvalue += c;
                        break;
                    case ARGVALUE: argvalue += c; break;
                    case ESCAPE: argvalue += getEscapedChar(c); currentState.pop(); break;
                }

            break;
        };
    }

    switch(currentState.top()) {
        case SCAN:
            if (argvalue.size()) {
                if (it == args.end())
                    throw ConvNameParserException("Too many arguments provided, need "s + std::to_string(args.size()));
                ret.setArgValue(it->mName, it->mArgType, argvalue);
            }
            break;
        case ARGVALUE:
            throw ConvNameParserException("Argument "s + std::to_string(ret.count()) + " is an unterminated string");
        case ESCAPE:
            throw ConvNameParserException("Argument "s + std::to_string(ret.count()) + " has an invalid escape sequence");
    }

    return ret;
}

}; //namespace
