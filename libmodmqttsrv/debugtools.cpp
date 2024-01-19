#include "debugtools.hpp"

#include <sstream>

namespace modmqttd {

std::string
DebugTools::registersToStr(const std::vector<uint16_t>& data) {
    std::stringstream ret;
    constexpr int maxVals = 10;
    int counter = maxVals;
    for(auto &val: data) {
        ret << "[" << std::hex << val << "]";
        counter--;
        if (counter == 0) {
            ret << " (… and " << data.size()-maxVals << " more)";
            break;
        }
    }

    return ret.str();
};

}
