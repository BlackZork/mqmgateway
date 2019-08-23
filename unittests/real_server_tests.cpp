#include "catch2/catch.hpp"
#include "libmodmqttsrv/modmqtt.hpp"

#include "mockedserver.hpp"
#include <thread>

//make sure that nothing listen on given ports
static const std::string config = R"(
modbus:
  networks:
    - name: tcptest
      address: localhost
      port: 64555
mqtt:
  client_id: mqttc
  broker:
    host: localhost
    port: 64666
  objects:
    - topic: test_switch
      commands:
        - name: set
          register: tcptest.1.1
          register_type: coil
      state:
        register: tcptest.1.1
        register_type: coil
      availability:
        register: tcptest.1.2
        register_type: bit
        available_value: 1
)";


TEST_CASE ("Start and stop real server that cannot connect to anything") {
    ModMqttServerThread server(config);
    server.start();
    // we need to sleep to let mqtt server to start waiting on gHasMessagesCondition
    // in mqtt initial connection loop
    // otherwise stop signal is missed and test will last for 5 seconds
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    server.stop();
}

