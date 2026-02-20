#include "conv_name_parser.hpp"

#include <exception>
#include <memory>
#include <regex>
#include <string>

#include "libmodmqttconv/convargs.hpp"

#include "exceptions.hpp"
#include "strutils.hpp"

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
        std::string mArgName;
        std::string mArgValue;
        ConverterArgs::const_iterator mCurrentPosArgIterator;
        std::vector<std::string> mProcessedArgs;

        void setArgValue(ConverterArgValues& values);
        char getEscapedChar(const char& c) { return c; };
        const std::string& validateArgName(const std::string& argName);
};

const std::string&
ConverterArgParser::validateArgName(const std::string& argName) {
    //TODO check for chars outside a-zA-Z0-9
    return argName;
};

void
ConverterArgParser::setArgValue(ConverterArgValues& values) {
    try {
        ConverterArgType argType = ConverterArgType::INVALID;
        if (!mArgName.empty()) {
            mUseArgOrder = false;
        } else {
            if (!mUseArgOrder)
                throw ConvNameParserException("Cannot use positional argument after named argument "s + std::to_string(mArgs.size()));

            if (mCurrentPosArgIterator == mArgs.end())
                throw ConvNameParserException("Too many arguments provided, need "s + std::to_string(mArgs.size()));
                mArgName = mCurrentPosArgIterator->mName;
            mCurrentPosArgIterator++;
        }

        auto it = std::find(mProcessedArgs.begin(), mProcessedArgs.end(), mArgName);
        if (it != mProcessedArgs.end())
            throw ConvNameParserException(mArgName + " already set");

        values.setArgValue(mArgName, mArgValue);
        mProcessedArgs.push_back(mArgName);
        mArgName.clear();
        mArgValue.clear();
    } catch (const std::exception& ex) {
        throw ConvNameParserException("Error setting argument " + mArgName + ":" + ex.what());
    }
};

ConverterArgValues
ConverterArgParser::parse(const std::string& argStr) {
    ConverterArgValues ret(mArgs);

    mProcessedArgs.clear();
    mCurrentPosArgIterator = mArgs.begin();
    mCurrentState.push(SCAN);

    char str_delimiter = 0x0;

    for(const char& c: argStr) {
        switch(c) {
            case '\\':
                switch(mCurrentState.top()) {
                    case SCAN: mCurrentState.push(ESCAPE); break;
                    case ARGVALUE: mArgValue += c; break;
                    case ESCAPE:
                        mArgValue += getEscapedChar(c);
                        mCurrentState.pop();
                    break;
                }
            break;
            case '=':
                switch(mCurrentState.top()) {
                    case SCAN:
                        if (mArgValue.empty())
                            throw ConvNameParserException("Missing name for argument " + std::to_string(ret.count() + 1));
                        if (!mArgName.empty())
                            throw ConvNameParserException("Name for argument " + std::to_string(ret.count() + 1) + " already set to " + mArgName);
                        mArgName = validateArgName(mArgValue);
                        mArgValue.clear();
                    break;
                    case ARGVALUE:
                        throw ConvNameParserException("Name for argument " + std::to_string(ret.count() + 1) + " cannot be quoted");
                    break;
                    case ESCAPE:
                        mArgValue += getEscapedChar(c);
                        mCurrentState.pop();
                    break;
                }
            break;
            case ',':
                switch(mCurrentState.top()) {
                    case SCAN:
                        if (mArgValue.empty())
                            throw ConvNameParserException("Argument " + std::to_string(ret.count() + 1) + " is empty");
                        setArgValue(ret);
                    break;
                    case ARGVALUE: mArgValue += c; break;
                    case ESCAPE:
                        mArgValue += getEscapedChar(c);
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
                            mArgValue += c;
                    break;
                    case ESCAPE:
                        mArgValue += getEscapedChar(c);
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
                            mArgValue += c;
                        break;
                    case ESCAPE:
                        mArgValue += getEscapedChar(c);
                        mCurrentState.pop();
                    break;
                }
            break;
            case ' ':
                switch(mCurrentState.top()) {
                    case SCAN: break;
                    case ARGVALUE: mArgValue += c; break;
                    case ESCAPE: mArgValue += getEscapedChar(c); mCurrentState.pop(); break;
                }
            break;
            default:
                switch(mCurrentState.top()) {
                    case SCAN:
                        mArgValue += c;
                        break;
                    case ARGVALUE: mArgValue += c; break;
                    case ESCAPE: mArgValue += getEscapedChar(c); mCurrentState.pop(); break;
                }

            break;
        };
    }

    switch(mCurrentState.top()) {
        case SCAN:
            if (mArgValue.size())
                setArgValue(ret);
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

    std::string val(spec);
    StrUtils::trim(val);

    std::cmatch matches;
    if (!std::regex_match(val.c_str(), matches, re))
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
