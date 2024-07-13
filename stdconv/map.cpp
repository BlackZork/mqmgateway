#include "map.hpp"

#include <stack>

class MapParser {
    public:
        static void parse(std::map<uint16_t, MapConverter::MqttVariant> mMap, const std::string& data);
    private:
        typedef enum {
            SCAN,
            KEY,
            ESCAPE,
            VALUE
        } aState;

};

char
getEscapedChar(const char& c) {
    return c;
};

void
MapParser::parse(std::map<uint16_t, MapConverter::MqttVariant> mMap, const std::string& data) {
    mMap.clear();

    std::string key;
    std::string value2;
    std::stack<aState> currentState;
    currentState.push(SCAN);

    for(const char& c: data) {
        switch(c) {
            case '{':
                //TODO ignore in scan
            break;
            case '}':
                //TODO end parsing, check for unknown chars
            break;
            case '\\':
                switch(currentState.top()) {
                    case SCAN:
                        currentState.push(ESCAPE);
                    break;
                    case KEY:
                        key += c;
                    case VALUE:
                        throw ConvException("\\ in map value is not allowed, must be an uint16_value");
                    break;
                    case ESCAPE:
                        key += getEscapedChar(c);
                        currentState.pop();
                    break;
                }
            break;
            case '"':
                switch(currentState.top()) {
                    case SCAN:
                        currentState.push(KEY); //TODO mark as string key
                    break;
                    case KEY:
                        currentState.pop();
                        break;
                    case ESCAPE:
                        key += getEscapedChar(c);
                        currentState.pop();
                    break;
                    case VALUE:
                        throw ConvException("\" in map value is not allowed, must be an uint16_value");
                    break;
                }
            break;
            case ' ':
                switch(currentState.top()) {
                    case SCAN: break;
                    case KEY: key += c; break;
                    case VALUE: break; //TODO not allowed
                    case ESCAPE: key += getEscapedChar(c); currentState.pop(); break;
                }
            case ':':
                switch(currentState.top()) {
                    case SCAN:
                        if (key.empty())
                            throw ConvException("Key " + std::to_string(mMap.size() + 1) + " is empty");
                        currentState.push(VALUE);
                        break;
                    case KEY: key += c; break;
                    case ESCAPE:
                        key += getEscapedChar(c);
                        currentState.pop();
                    break;
                    case VALUE:
                        throw ConvException(": in map value is not allowed, must be an uint16_value");
                    break;
                }
            case ',':
                switch(currentState.top()) {
                    case SCAN:
                        throw ConvException("Key " + std::to_string(mMap.size() + 1) + " is empty");
                    case KEY: key += c; break;
                    case VALUE: {
                        MapConverter::MqttVariant k;
                        k.mStr = new char[key.size() + 1];
                        strcpy(k.mStr, key.c_str());
                        uint16_t v = Int16Converter::toInt16(MqttValue::fromString(value2));
                        //map is optimized for polling, so modbus value is stored as key.
                        auto it = mMap.find(v);
                        if (it != mMap.end())
                            throw ConvException("Key " + std::to_string(v) + " already exists");
                        mMap[v] = k;
                        break;
                    }
                    case ESCAPE:
                        key += getEscapedChar(c);
                        currentState.pop();
                    break;
                }
            break;
            default:
                //TODO mark as int key if 0-9 in scan,
                //TODO check for isDigit in value
            break;
        }


    }
}


void
MapConverter::parseMap(const std::string& pStrMap) {

}

std::map<uint16_t, MapConverter::MqttVariant>::const_iterator
MapConverter::findVariant(const MqttValue& pValue) const {
    if (mIsIntMap) {
        int32_t val = pValue.getInt();
        for (auto it = mMap.begin(); it != mMap.end(); ++it)
            if (it->second.mInt == val)
                return it;
    } else {
        std::string val = pValue.getString();
        for (auto it = mMap.begin(); it != mMap.end(); ++it)
            if (it->second.mStr == val.c_str())
                return it;
    }
    return mMap.end();
}
