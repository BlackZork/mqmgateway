
#include "libmodmqttsrv/imodbuscontext.hpp"

class MockedModbusWatchdog : modmqttd::IModbusWatchdog {
    public:
        int getFailedSerialPortCheckCount() { return mFailedSerialPortCheckCount; }

    private:
        int mFailedSerialPortCheckCount;
};
