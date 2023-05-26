#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include "exceptions.hpp"

namespace modmqttd {

class DebugTools {
    public:
        static std::string registersToStr(const std::vector<uint16_t>& data);
};

class DebugException : ModMqttException {
    public:
        DebugException(const std::string& what) : ModMqttException(what) {}

};


}
