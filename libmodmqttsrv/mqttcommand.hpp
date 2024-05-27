#pragma once

#include "modbus_messages.hpp"
#include "libmodmqttconv/converter.hpp"

#include "mqttobject.hpp" //temporary

namespace modmqttd {

class MqttObjectCommand : public ModbusSlaveAddressRange {
    public:
        enum PayloadType {
            STRING = 1
        };
        MqttObjectCommand(
            int pCommandId,
            const std::string& pTopic,
            PayloadType pPayloadType,
            const std::string& pModbusNetworkName,
            int pSlaveId,
            RegisterType pRegisterType,
            int pRegisterNumber,
            int pRegisterCount = 1
        ) : ModbusSlaveAddressRange(
                pSlaveId, pRegisterNumber, pRegisterType, pRegisterCount
            ),
            mTopic(pTopic),
            mPayloadType(pPayloadType),
            mModbusNetworkName(pModbusNetworkName),
            mCommandId(pCommandId)
        {};
        std::string mTopic;
        PayloadType mPayloadType;
        std::string mModbusNetworkName;

        void setConverter(std::shared_ptr<DataConverter> conv) { mConverter = conv; }
        bool hasConverter() const { return mConverter != nullptr; }
        const DataConverter& getConverter() const { return *mConverter; }
        int getCommandId() const { return mCommandId; }
    private:
        int mCommandId;
        std::shared_ptr<DataConverter> mConverter;
};

}
