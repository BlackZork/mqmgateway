#include "catch2/catch_all.hpp"
#include "mockedserver.hpp"
#include "defaults.hpp"
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
    SECTION("is not defined register value is published on change only") {
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 1);
        server.start();
        server.waitForPublish("test_sensor/state");
        REQUIRE(server.mqttValue("test_sensor/state") == "1");
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
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
        std::this_thread::sleep_for(std::chrono::milliseconds(60));
        server.stop();
        int count = server.mMqtt->getPublishCount("test_sensor/state");
        REQUIRE(count > 2);
    }

    SECTION("is set to every_poll on topic level register value is published after each read") {
        config.mYAML["mqtt"]["objects"][0]["publish_mode"] = "every_poll";
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 2);
        server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, 3);
        server.start();
        server.waitForPublish("test_sensor/state");
        REQUIRE(server.mqttValue("test_sensor/state") == "1");
        std::this_thread::sleep_for(std::chrono::milliseconds(70));
        server.stop();

        int other_count = server.mMqtt->getPublishCount("other_sensor/state");
        REQUIRE(other_count == 1);

        int test_count = server.mMqtt->getPublishCount("other_sensor/state");
        REQUIRE(test_count > 2);

    }
}

