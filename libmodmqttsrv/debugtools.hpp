#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace modmqttd {

class DebugTools {
    public:
        static std::string registersToStr(const std::vector<uint16_t>& data);
};


}
