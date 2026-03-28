#include <catch2/catch_all.hpp>
#include "mockedserver.hpp"
#include "yaml_utils.hpp"


TEST_CASE ("Refresh") {

TestConfig config(R"(
modmqttd:
  log_level: trace
modbus:
  networks:
    - name: tcptest
      address: localhost
      port: 501
mqtt:
  client_id: mqtt_test
  publish_mode: every_poll
  broker:
    host: localhost
  objects:
    - topic: test_switch
      state:
        register: tcptest.1.1
        register_type: coil
)");

    SECTION ("should include register read time") {
        config.mYAML["mqtt"]["refresh"] = std::to_string(timing::milliseconds(60).count()) + "ms";
        MockedModMqttServerThread server(config.toString());
        server.setSlaveReadTime("tcptest", 1, timing::milliseconds(40));
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::BIT, false);
        server.start();
        std::this_thread::sleep_for(timing::milliseconds(180));
        server.stop();
        REQUIRE(server.getMockedModbusContext("tcptest").getReadCount(1) == 3);
    }
}
