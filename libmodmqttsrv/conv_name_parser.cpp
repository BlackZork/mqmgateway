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

char
getEscapedChar(const char& c) {
    return c;
};

ConverterArgValues
ConverterNameParser::parseArgs(const ConverterArgs& args, const std::string& argSpec) {
    ConverterArgValues ret;

    ConverterArgs::const_iterator it = args.begin();

    std::stack<aState> currentState;
    currentState.push(SCAN);

    std::string arg_value;
    char str_delimiter = 0x0;

    for(const char& c: argSpec) {
        switch(c) {
            case '\\':
                switch(currentState.top()) {
                    case SCAN: currentState.push(ESCAPE); break;
                    case STRING: arg_value += c; break;
                    case ESCAPE:
                        arg_value += getEscapedChar(c);
                        currentState.pop();
                    break;
                }
            break;
            case ',':
                switch(currentState.top()) {
                    case SCAN:
                        if (arg_value.empty())
                            throw ConvNameParserException("Argument " + std::to_string(ret.count() + 1) + " is empty");

                        if (it == args.end())
                            throw ConvNameParserException("Too many arguments provided, need "s + std::to_string(args.size()));
                        ret.addArgValue(it->mName, it->mArgType, arg_value);
                        it++;
                        arg_value.clear();

                        break;
                    case STRING: arg_value += c; break;
                    case ESCAPE:
                        arg_value += getEscapedChar(c);
                        currentState.pop();
                    break;
                }
            break;
            case '"':
                switch(currentState.top()) {
                    case SCAN:
                        currentState.push(STRING);
                        str_delimiter = c;
                    break;
                    case STRING:
                        if (str_delimiter == c)
                            currentState.pop();
                        else
                            arg_value += c;
                        break;
                    case ESCAPE:
                        arg_value += getEscapedChar(c);
                        currentState.pop();
                    break;
                }
            break;
            case '\'':
                switch(currentState.top()) {
                    case SCAN:
                        currentState.push(STRING);
                        str_delimiter = c;
                    break;
                    case STRING:
                        if (str_delimiter == c)
                            currentState.pop();
                        else
                            arg_value += c;
                        break;
                    case ESCAPE:
                        arg_value += getEscapedChar(c);
                        currentState.pop();
                    break;
                }
            break;
            case ' ':
                switch(currentState.top()) {
                    case SCAN: break;
                    case STRING: arg_value += c; break;
                    case ESCAPE: arg_value += getEscapedChar(c); currentState.pop(); break;
                }
            break;
            default:
                switch(currentState.top()) {
                    case SCAN:
                        arg_value += c;
                        break;
                    case STRING: arg_value += c; break;
                    case ESCAPE: arg_value += getEscapedChar(c); currentState.pop(); break;
                }

            break;
        };
    }

    switch(currentState.top()) {
        case SCAN:
            if (arg_value.size()) {
                if (it == args.end())
                    throw ConvNameParserException("Too many arguments provided, need "s + std::to_string(args.size()));
                ret.addArgValue(it->mName, it->mArgType, arg_value);
            }
            break;
        case STRING:
            throw ConvNameParserException("Argument "s + std::to_string(ret.count()) + " is an unterminated string");
        case ESCAPE:
            throw ConvNameParserException("Argument "s + std::to_string(ret.count()) + " has an invalid escape sequence");
    }

    return ret;
}

}; //namespace
