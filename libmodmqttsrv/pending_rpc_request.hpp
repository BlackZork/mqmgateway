#pragma once

#include <memory>
#include <string>

#include "libmodmqttconv/converter.hpp"
#include "modbus_types.hpp"

namespace modmqttd {

class CorrelationData {
    public:
        CorrelationData() = default;
        CorrelationData(const std::shared_ptr<void>& pData, int pLen)
            : mData(pData), mLen(pLen) {}
        const void* data() const { return mData.get(); }
        int size() const { return mLen; }

    private:
        std::shared_ptr<void> mData;
        int mLen = 0;
};

struct PendingRpcRequest {
        std::string mNetworkName;
        std::string mResponseTopic;
        CorrelationData mCorrelationData;
        int mSlaveId = 0;
        int mRegisterNumber = 0;
        std::string mDisplayAddress;
        RegisterType mRegisterType = RegisterType::HOLDING;
        int mCount = 1;
        bool mIsWrite = false;
        // optional converter for a read reply; null ⇒ raw register value(s)
        std::shared_ptr<DataConverter> mConverter;
};

}
