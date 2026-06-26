#pragma once

#include "modbus_messages.hpp"
#include "libmodmqttconv/converter.hpp"

#include "mqttobject.hpp" //temporary

namespace modmqttd {

class MqttObjectCommand : public ModbusRequestBase {
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
            int pRegisterCount,
            ModbusWriteMode pWriteMode) : ModbusRequestBase(pSlaveId, pRegisterNumber, pRegisterType, pRegisterCount, pCommandId),
                                          mTopic(pTopic),
                                          mPayloadType(pPayloadType),
                                          mModbusNetworkName(pModbusNetworkName),
                                          mWriteMode(pWriteMode) {}

        std::string mTopic;
        PayloadType mPayloadType;
        std::string mModbusNetworkName;
        ModbusWriteMode mWriteMode;

        void setConverter(std::shared_ptr<DataConverter> conv) { mConverter = conv; }
        bool hasConverter() const { return mConverter != nullptr; }
        const DataConverter& getConverter() const { return *mConverter; }

    private:
        std::shared_ptr<DataConverter> mConverter;
};

}
