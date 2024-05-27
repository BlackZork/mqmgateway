#pragma once

#include "mqttobject.hpp"

namespace modmqttd {

class MqttPayload {
    public:
        static std::string generate(const MqttObject& pObj);

};

}
