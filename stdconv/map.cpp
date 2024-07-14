#include "map.hpp"

#include <cctype>

#include "libmodmqttconv/strutils.hpp"

char
getEscapedChar(const char& c) {
    return c;
};

void
MapParser::parse(MapConverter::Map& mappings, const std::string& data) {
    mappings.clear();

    mIsIntKey = false;
    mCurrentState = std::stack<aState>();
    mCurrentState.push(SCAN);

    for(auto it = data.cbegin(); it != data.cend(); it++) {
        const char& c = *it;
        switch(c) {
            case '{':
                switch(mCurrentState.top()) {
                    case SCAN:
                        // just ignore it for simplicty
                    break;
                    case KEY:
                        mKey += c;
                    case VALUE:
                        throw ConvException("\\ in map value is not allowed, must be an uint16_value");
                    break;
                    case ESCAPE:
                        addEscapedChar(c);
                    break;
                }
            break;
            case '}':
                switch(mCurrentState.top()) {
                    case SCAN:
                        throw ConvException("Missing map opening bracket");
                    break;
                    case KEY:
                        mKey += c;
                    case VALUE:
                        addMapping(mappings);
                    break;
                    case ESCAPE:
                        addEscapedChar(c);
                    break;
                }
            break;
            case '\\':
                switch(mCurrentState.top()) {
                    case SCAN:
                        throw ConvException("string key should start with \"");
                    break;
                    case KEY:
                        mKey += c;
                    case VALUE:
                        throw ConvException("\\ in map value is not allowed, must be an uint16_value");
                    break;
                    case ESCAPE:
                        addEscapedChar(c);
                    break;
                }
            break;
            case '"':
                switch(mCurrentState.top()) {
                    case SCAN:
                        mCurrentState.push(KEY);
                        mIsIntKey = false;
                    break;
                    case KEY:
                        if (!mIsIntKey)
                            mCurrentState.pop();
                        else
                            throw ConvException("\" in int key is not allowed");
                    break;
                    case ESCAPE:
                        addEscapedChar(c);
                    break;
                    case VALUE:
                        throw ConvException("\" in map value is not allowed, must be an uint16_value");
                    break;
                }
            break;
            case ' ':
                switch(mCurrentState.top()) {
                    case SCAN:
                    break;
                    case KEY:
                        if (!mKey.empty())
                            mKey += c;
                    break;
                    case VALUE:
                        if (!mValue.empty())
                            throw ConvException("space in map value is not allowed, must be an uint16_value");
                    break;
                    case ESCAPE:
                        addEscapedChar(c);
                    break;
                }
            break;
            case ':':
                switch(mCurrentState.top()) {
                    case SCAN:
                        throw ConvException("Key " + std::to_string(mappings.size() + 1) + " is empty");
                    break;
                    case KEY:
                        mCurrentState.pop();
                        mCurrentState.push(VALUE);
                    break;
                    case ESCAPE:
                        addEscapedChar(c);
                    break;
                    case VALUE:
                        throw ConvException(": in map value is not allowed, must be an uint16_value");
                    break;
                }
            break;
            case ',':
                switch(mCurrentState.top()) {
                    case SCAN:
                        throw ConvException("Key " + std::to_string(mappings.size() + 1) + " is empty");
                    case KEY:
                        mKey += c;
                    break;
                    case VALUE:
                        addMapping(mappings);

                    break;
                    case ESCAPE:
                        addEscapedChar(c);
                    break;
                }
            break;
            default:
                switch(mCurrentState.top()) {
                    case SCAN:
                        if (isdigit(c)) {
                            mIsIntKey = true;
                            mCurrentState.push(KEY);
                            mKey += c;
                        } else {
                            throw ConvException("string key should start with \"");
                        }
                    break;
                    case KEY:
                        if (mIsIntKey) {
                            if (!isdigit(c) && c != 'x')
                                throw ConvException("Invalid char " + std::to_string(c) + " in int key");
                        } else {
                            mKey += c;
                        }
                    case VALUE:
                        mValue += c;
                    break;
                    case ESCAPE:
                        addEscapedChar(c);
                    break;
                }
            break;
        }


    }
}

void
MapParser::addEscapedChar(char c) {
    char escaped =getEscapedChar(c);
    mCurrentState.pop();
    switch (mCurrentState.top()) {
        case KEY:
            mKey += escaped;
        break;
        case VALUE:
            mValue += escaped;
        break;
        default:
            throw ConvException("Internal parser error: invalid state " + std::to_string(mCurrentState.top()) + " when adding escaped char " + escaped);
    };
}

void
MapParser::addMapping(MapConverter::Map& mappings) {
    uint16_t v = Int16Converter::toInt16(MqttValue::fromString(mValue));
    //map is optimized for polling, so modbus value is stored as key.
    auto vit = mappings.findRegValue(v);
    if (vit != mappings.end())
        throw ConvException("Register value " + std::to_string(v) + " already mapped");

    MapConverter::Mapping mapping;
    if (mIsIntKey) {
        mapping = MapConverter::Mapping(v, std::atoi(mKey.c_str()));
    } else {
        mapping = MapConverter::Mapping(v, StrUtils::trim(mKey));
    }

    vit = std::find_if(mappings.begin(), mappings.end(), [&mapping](const MapConverter::Mapping& m) -> bool {
        if (mapping.isInt()) {
            if (!m.isInt())
                return false;
            return mapping.getIntMqttValue() == m.getIntMqttValue();
        } else {
            if (m.isInt())
                return true;
            return strcmp(mapping.getStrMqttValue(), m.getStrMqttValue()) == 0;
        }
        return false; //unreacheable
    });

    if (vit != mappings.end())
        throw ConvException("Mqtt value " + mValue + " already mapped");

    mappings.push_back(mapping);
    mCurrentState.pop();
}


void
MapConverter::parseMap(const std::string& pStrMap) {
    MapParser p;
    p.parse(mMappings, pStrMap);
}

std::vector<MapConverter::Mapping>::const_iterator
MapConverter::Map::findVariant(const MqttValue& pValue) const {
    for (auto it = begin(); it != end(); ++it) {
        if (it->isInt()) {
            int32_t val = pValue.getInt();
            if (it->getIntMqttValue() == val)
                return it;
        } else {
            std::string val = pValue.getString();
            if (it->getStrMqttValue() == val)
                return it;
        }
    }
    return end();
}
