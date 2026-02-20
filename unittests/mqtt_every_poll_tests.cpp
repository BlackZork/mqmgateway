#include "catch2/catch_all.hpp"
#include "mockedserver.hpp"
#include "timing.hpp"
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
        std::this_thread::sleep_for(timing::milliseconds(80));
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
        std::this_thread::sleep_for(timing::milliseconds(80));
        server.stop();
        // should respect 1.2 10ms refresh
        // which gives at least 4 mqtt messages in 80ms
        REQUIRE(server.getPublishCount("test_sensor/state") >= 4);
    }

}

TEST_CASE ("every_poll for two topics") {

TestConfig config(R"(
modbus:
  networks:
    - name: tcptest
      address: localhost
      port: 501
      slaves:
        - address: 1
          poll_groups:
            - register: 1
              register_type: holding
              count: 10
mqtt:
  client_id: mqtt_test
  refresh: 10ms
  broker:
    host: localhost
  objects:
    - topic: test_sensor1
      publish_mode: on_change
      state:
        register: tcptest.1.2
    - topic: test_sensor2
      publish_mode: every_poll
      state:
        register: tcptest.1.3
)");
    SECTION("should be set when every_poll is merged to on_change") {
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 1);
        server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, 2);
        server.start();

        std::this_thread::sleep_for(timing::milliseconds(80));
        server.stop();
        REQUIRE(server.getPublishCount("test_sensor2/state") >= 4);
    }

    SECTION("should be preserved when on_change is merged to every_poll") {
        config.mYAML["mqtt"]["objects"][0]["publish_mode"] = "every_poll";
        config.mYAML["mqtt"]["objects"][1]["publish_mode"] = "on_change";
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 1);
        server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, 2);
        server.start();

        std::this_thread::sleep_for(timing::milliseconds(80));
        server.stop();
        REQUIRE(server.getPublishCount("test_sensor1/state") >= 4);
    }

}

