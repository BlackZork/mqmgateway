#include "catch2/catch_all.hpp"
#include "yaml_utils.hpp"
#include "mockedserver.hpp"

TEST_CASE ("Write retry") {
TestConfig config(R"(
modbus:
  networks:
    - name: tcptest
      address: localhost
      port: 501
      slaves:
        - address: 1
          delay_before_command: 50ms
mqtt:
  client_id: mqtt_test
  broker:
    host: localhost
  objects:
    - topic: write_fail
      commands:
       - name: set
         register: tcptest.1.2
         register_type: holding
)");

SECTION ("should issue many write calls if write fails") {
    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 0);
    server.mModbusFactory->setModbusRegisterWriteError("tcptest", 1, 2, modmqttd::RegisterType::HOLDING);

    server.start();

    server.waitForSubscription("write_fail/set");
    server.publish("write_fail/set", "7");

    // delay before command is 50ms, should try at least 2 times in this period
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    server.stop();

    REQUIRE(server.getMockedModbusContext("tcptest").getWriteCount(1) > 1);

}

}

