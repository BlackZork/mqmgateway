#include "map.hpp"

#include <cctype>

char
getEscapedChar(const char& c) {
    return c;
};

void
MapParser::parse(MapConverter::Map& mappings, const std::string& data) {
    mappings.clear();

    mValueType = aValueType::NONE;
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
                        throw ConvException("{ in map key is not allowed, must be an uint16_value");
                    case VALUE:
                        mValue += c;
                    break;
                    case ESCAPE:
                        addEscapedChar(c);
                    break;
                }
            break;
            case '}':
                switch(mCurrentState.top()) {
                    case SCAN:
                        if (mValueType == aValueType::INT)
                            addMapping(mappings);
                    break;
                    case KEY:
                        throw ConvException("} in map key is not allowed, must be an uint16_value");
                    case VALUE:
                        switch(mValueType) {
                            case aValueType::INT: addMapping(mappings); break;
                            case aValueType::STRING: mValue += c;
                            default:
                                throw ConvException("Internal parser error: unknown value type on closing brace");
                        }
                    break;
                    case ESCAPE:
                        addEscapedChar(c);
                    break;
                }
            break;
            case '\\':
                switch(mCurrentState.top()) {
                    case SCAN:
                    case KEY:
                        throw ConvException("\\ in map key is not allowed, must be an uint16_value");
                    break;
                    case VALUE:
                        mCurrentState.push(ESCAPE);
                    break;
                    case ESCAPE:
                        addEscapedChar(c);
                    break;
                }
            break;
            case '"':
                switch(mCurrentState.top()) {
                    case SCAN:
                        if (mKey.empty())
                            mCurrentState.push(KEY);
                        else {
                            mCurrentState.push(VALUE);
                            mValueType = aValueType::STRING;
                        }
                    break;
                    case KEY:
                        throw ConvException("\" in map key is not allowed, must be an uint16_value");
                    break;
                    case ESCAPE:
                        addEscapedChar(c);
                    break;
                    case VALUE:
                        switch(mValueType) {
                            case aValueType::NONE: mValueType = aValueType::STRING; break;
                            case aValueType::STRING: addMapping(mappings); break;
                            case aValueType::INT:
                                throw ConvException("\" in int value is not allowed");
                            break;
                        }
                    break;
                }
            break;
            case ' ':
                switch(mCurrentState.top()) {
                    case SCAN:
                    break;
                    case KEY:
                        mCurrentState.pop();
                    break;
                    case VALUE:
                        if (mValueType == aValueType::INT)
                            mCurrentState.pop();
                        else if (mValueType == aValueType::STRING)
                            mValue += c;
                    break;
                    case ESCAPE:
                        addEscapedChar(c);
                    break;
                }
            break;
            case ':':
                switch(mCurrentState.top()) {
                    case SCAN:
                        if (mKey.empty())
                            throw ConvException("Key " + std::to_string(mappings.size() + 1) + " is empty");
                    break;
                    case KEY:
                        mCurrentState.pop();
                    break;
                    case ESCAPE:
                        addEscapedChar(c);
                    break;
                    case VALUE:
                        mValue += c;
                    break;
                }
            break;
            case ',':
                switch(mCurrentState.top()) {
                    case SCAN:
                    break;
                    case KEY:
                        throw ConvException(", in map key is not allowed, must be an uint16_value");
                    break;
                    case VALUE:
                        if (mValueType == aValueType::INT)
                            addMapping(mappings);
                        else
                            mValue += c;
                    break;
                    case ESCAPE:
                        addEscapedChar(c);
                    break;
                }
            break;
            default:
                switch(mCurrentState.top()) {
                    case SCAN:
                        if (mKey.empty()) {
                            if (isdigit(c)) {
                                mCurrentState.push(KEY);
                                mKey += c;
                            } else {
                                throw ConvException("string key should start with \"");
                            }
                        } else {
                            if (mValueType == aValueType::NONE && isdigit(c))
                                mValueType = aValueType::INT;
                            mCurrentState.push(VALUE);
                            mValue += c;
                        }
                    break;
                    case KEY:
                        if (!isdigit(c) && c != 'x')
                            throw ConvException("Invalid char " + std::to_string(c) + " in int key");
                        mKey += c;
                    break;
                    case VALUE:
                        if (mValueType == aValueType::NONE && isdigit(c))
                            mValueType = aValueType::INT;
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
    char escaped = getEscapedChar(c);
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
    if (mKey.empty())
        throw ConvException("Internal parser error: register value cannot be empty");

    uint16_t regVal = MqttValue::fromString(mKey).getUInt16();
    //map is optimized for polling, so modbus value is stored as key.
    auto vit = mappings.findRegValue(regVal);
    if (vit != mappings.end())
        throw ConvException("Register value " + mKey + " already mapped");

    MapConverter::Mapping mapping;
    if (mValueType == aValueType::INT) {
        mapping = MapConverter::Mapping(regVal, std::atoi(mValue.c_str()));
    } else {
        mapping = MapConverter::Mapping(regVal, mValue);
    }

    vit = std::find_if(mappings.begin(), mappings.end(), [&mapping](const MapConverter::Mapping& m) -> bool {
        if (mapping.isInt()) {
            if (!m.isInt())
                return false;
            return mapping.getIntMqttValue() == m.getIntMqttValue();
        } else {
            if (m.isInt())
                return false;
            return strcmp(mapping.getStrMqttValue(), m.getStrMqttValue()) == 0;
        }
        return false; //unreacheable
    });

    if (vit != mappings.end())
        throw ConvException("Mqtt value " + mValue + " already mapped");

    mappings.push_back(mapping);
    mValueType = aValueType::NONE;
    mKey.clear();
    mValue.clear();
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
