#include "conv_name_parser.hpp"

#include <regex>

#include "exceptions.hpp"

namespace modmqttd {

ConverterSpecification
ConverterNameParser::parse(const std::string& spec) {
    std::regex re(RE_CONV);

    std::cmatch matches;
    if (!std::regex_match(spec.c_str(), matches, re))
        throw ConvNameParserException("Supply converter spec in form: plugin.converter(arg1, arg2, …)");

    ConverterSpecification ret;

    ret.plugin = matches[1];
    ret.converter = matches[2];

    std::string args = matches[3];
    if (args != "()") {
        ret.args = parseArgs(args);
    }

    return ret;
}

char
getEscapedChar(const char& c) {
    return c;
};

std::vector<std::string>
ConverterNameParser::parseArgs(const std::string& argSpec) {
    std::vector<std::string> ret;

    std::stack<aState> currentState;
    currentState.push(SCAN);

    std::string arg;

    for(const char& c: argSpec) {
        switch(c) {
            case '\\':
                switch(currentState.top()) {
                    case SCAN: currentState.push(ESCAPE); break;
                    case STRING: arg += c; break;
                    case ESCAPE: arg += getEscapedChar(c); currentState.pop(); break;
                }
            break;
            case ',':
                switch(currentState.top()) {
                    case SCAN:
                        if (arg.empty())
                            throw ConvNameParserException("Argument " + std::to_string(ret.size() + 1) + " is empty");
                        ret.push_back(arg);
                        arg.clear();
                        break;
                    case STRING: arg += c; break;
                    case ESCAPE: arg += getEscapedChar(c); currentState.pop(); break;
                }
            break;
            case '"':
                switch(currentState.top()) {
                    case SCAN: currentState.push(STRING); break;
                    case STRING:
                        currentState.pop();
                        break;
                    case ESCAPE: arg += getEscapedChar(c); currentState.pop(); break;
                }
            break;
            case ' ':
                switch(currentState.top()) {
                    case SCAN: break;
                    case STRING: arg += c; break;
                    case ESCAPE: arg += getEscapedChar(c); currentState.pop(); break;
                }
            break;
            default:
                switch(currentState.top()) {
                    case SCAN:
                        arg += c;
                        break;
                    case STRING: arg += c; break;
                    case ESCAPE: arg += getEscapedChar(c); currentState.pop(); break;
                }

            break;
        };
    }

    switch(currentState.top()) {
        case SCAN:
            if (arg.size())
                ret.push_back(arg);
            break;
        case STRING:
            throw ConvNameParserException("Argument " + std::to_string(ret.size() + 1) + " is an unterminated string");
        case ESCAPE:
            throw ConvNameParserException("Argument " + std::to_string(ret.size() + 1) + " has an invalid escape sequence");
    }

    return ret;
}

}; //namespace
