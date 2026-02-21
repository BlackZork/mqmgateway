#include "catch2/catch_all.hpp"
#include "mockedserver.hpp"
#include "yaml_utils.hpp"

TEST_CASE ("When publish_type") {

TestConfig config(R"(
modbus:
  networks:
    - name: tcptest
      address: localhost
      port: 501
mqtt:
  client_id: mqtt_test
  refresh: 10ms
  broker:
    host: localhost
  objects:
    - topic: test_sensor
      state:
        register: tcptest.1.2
    - topic: other_sensor
      state:
        register: tcptest.1.3
)");
    SECTION("is not defined then register value is published on change only") {
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 1);
        server.start();
        server.waitForPublish("test_sensor/state");
        REQUIRE(server.mqttValue("test_sensor/state") == "1");
        std::this_thread::sleep_for(timing::milliseconds(50));
        server.stop();
        server.requirePublishCount("test_sensor/state", 1);
    }

    SECTION("is on_change then register value is published on change only") {
        config.mYAML["mqtt"]["publish_mode"] = "on_change";
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 1);
        server.start();
        server.waitForPublish("test_sensor/state");
        REQUIRE(server.mqttValue("test_sensor/state") == "1");
        std::this_thread::sleep_for(timing::milliseconds(50));
        server.stop();
        server.requirePublishCount("test_sensor/state", 1);
    }

    SECTION("is set to every_poll on mqtt level register value is published after each read") {
        config.mYAML["mqtt"]["publish_mode"] = "every_poll";
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 1);
        server.start();
        server.waitForPublish("test_sensor/state");
        REQUIRE(server.mqttValue("test_sensor/state") == "1");
        std::this_thread::sleep_for(timing::milliseconds(60));
        server.stop();
        int count = server.mMqtt->getPublishCount("test_sensor/state");
        REQUIRE(count > 2);
    }

    SECTION("is set to every_poll on topic level register value is published after each read") {
        config.mYAML["mqtt"]["objects"][1]["publish_mode"] = "every_poll";
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 2);
        server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, 3);
        server.start();
        server.waitForPublish("test_sensor/state");
        REQUIRE(server.mqttValue("test_sensor/state") == "2");
        std::this_thread::sleep_for(timing::milliseconds(70));
        server.stop();

        int test_count = server.mMqtt->getPublishCount("test_sensor/state");
        REQUIRE(test_count == 1);

        int other_count = server.mMqtt->getPublishCount("other_sensor/state");
        REQUIRE(other_count > 2);

    }

    SECTION ("is set to once then registers are not refreshed") {
        config.mYAML["mqtt"]["publish_mode"] = "once";
        MockedModMqttServerThread server(config.toString());
        server.start();
        server.waitForPublish("test_sensor/state");
        std::this_thread::sleep_for(timing::milliseconds(50));
        server.stop();
        // both registers should be read once
        REQUIRE(server.getMockedModbusContext("tcptest").getReadCount(1) == 2);
    }

    //TODO test for ONCE when publish failed or broker is disconnected
}

