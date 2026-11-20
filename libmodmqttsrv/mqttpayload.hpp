#pragma once

#include "mqttobject.hpp"

namespace modmqttd {

class MqttPayload {
    public:
        static std::string generate(const MqttObject& pObj);

        /**
         * A plain (non-JSON) payload: the value's text, or, for NaN and the
         * infinities, their encoding in pMode, which is what the value would
         * be as a JSON payload of its own.
         */
        static std::string encodeScalar(const MqttValue& pValue, InfNanMode pMode);
};

}
