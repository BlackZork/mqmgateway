#include "catch2/catch_all.hpp"
#include "mockedserver.hpp"
#include "defaults.hpp"
#include "yaml_utils.hpp"

TEST_CASE ("every_poll unnamed list") {

TestConfig config(R"(
modbus:
  networks:
    - name: tcptest
      address: localhost
      port: 501
mqtt:
  client_id: mqtt_test
  refresh: 50ms
  broker:
    host: localhost
  objects:
    - topic: test_sensor
      publish_mode: every_poll
      state:
        registers:
            - register: tcptest.1.2
            - register: tcptest.1.3
)");
    SECTION("should be published once per topic refresh period") {
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 1);
        server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, 2);
        server.start();

        server.waitForPublish("test_sensor/state");
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
        server.stop();
        // initial poll + one publish after 50ms
        server.requirePublishCount("test_sensor/state", 2);
    }

    SECTION("should be published once per register refresh period") {
        config.mYAML["mqtt"]["objects"][0]["state"]["registers"][0]["refresh"] = "10ms";
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 1);
        server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, 2);
        server.start();

        server.waitForPublish("test_sensor/state");
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
        server.stop();
        // should respect 1.3 refresh
        REQUIRE(server.getPublishCount("test_sensor/state") > 6);
    }

}
