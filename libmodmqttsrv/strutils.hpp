#pragma once

#include <algorithm>
#include <cctype>
#include <locale>

namespace modmqttd {

class StrUtils {
    public:
        inline static std::string& ltrim(std::string &s) {
            s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
                return !std::isspace(ch);
            }));
            return s;
        }

        // Trim from the end (in place)
        inline static std::string& rtrim(std::string &s) {
            s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
                return !std::isspace(ch);
            }).base(), s.end());
            return s;
        }

        inline static std::string& trim(std::string &s) {
            rtrim(s);
            ltrim(s);
            return s;
        }
};

}
