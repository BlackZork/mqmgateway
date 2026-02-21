#include "catch2/catch_all.hpp"
#include "mockedserver.hpp"
#include "yaml_utils.hpp"

TEST_CASE ("Retain flag") {

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
)");

    SECTION("set to true should trigger publish after initial poll") {
        config.mYAML["mqtt"]["refresh"] = "100ms";
        config.mYAML["mqtt"]["objects"][0]["retain"] = "true";
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 1);
        std::chrono::time_point<std::chrono::steady_clock> initialPollStart = std::chrono::steady_clock::now();
        server.start();
        server.waitForPublish("test_sensor/state");
        // ensure that this was the first poll
        auto end = std::chrono::steady_clock::now();
        auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - initialPollStart);
        REQUIRE(dur <  timing::milliseconds(50));
    }

    SECTION("set to false") {
        config.mYAML["mqtt"]["objects"][0]["retain"] = "false";

        SECTION("should trigger null value publish after initial poll") {
            MockedModMqttServerThread server(config.toString());
            server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 1);
            server.start();
            server.waitForInitialPoll("tcptest");
            // wait some time for mqttclient thread to process initial poll messages
            server.waitForPublish("test_sensor/state");
            REQUIRE(server.mqttNullValue("test_sensor/state"));

            server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 2);
            server.waitForPublish("test_sensor/state");
            REQUIRE(server.mqttValue("test_sensor/state") == "2");
        }

        SECTION("should trigger state publish after initial poll for every_poll") {
            config.mYAML["mqtt"]["refresh"] = "20ms";
            config.mYAML["mqtt"]["objects"][0]["publish_mode"] = "every_poll";
            MockedModMqttServerThread server(config.toString());
            server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 2);
            server.start();
            //5 ms initial poll read, 20ms wait, 5ms second read, 10ms test tolerance
            std::this_thread::sleep_for(timing::milliseconds(40));
            server.stop();

            // retained messsage delete and two state publishes
            int test_count = server.mMqtt->getPublishCount("test_sensor/state");
            REQUIRE(test_count == 3);
        }

        SECTION("should not publish state if availability is unset") {
            MockedModMqttServerThread server(config.toString());
            server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 1);
            server.start();

            server.waitForPublish("test_sensor/state");
            REQUIRE(server.mqttNullValue("test_sensor/state"));

            server.waitForPublish("test_sensor/availability");
            REQUIRE(server.mqttValue("test_sensor/availability") == "1");

            // do not publish 1 -> 2 change because availability is 0
            server.setModbusRegisterReadError("tcptest", 1, 2, modmqttd::RegisterType::HOLDING);
            server.waitForPublish("test_sensor/availability");
            REQUIRE(server.mqttValue("test_sensor/availability") == "0");

            server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 2);
            server.clearModbusRegisterReadError("tcptest", 1, 2, modmqttd::RegisterType::HOLDING);
            // make sure that there is at least one sucessfull
            // poll that is not published
            std::this_thread::sleep_for(timing::milliseconds(35));

            server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 3);

            server.waitForPublish("test_sensor/state");
            REQUIRE(server.mqttValue("test_sensor/state") == "3");
            // retain delete after inital poll and 2 -> 3 change only
            server.requirePublishCount("test_sensor/state", 2);
        }
    }
}

TEST_CASE ("Retain flag that is a part of poll group") {

TestConfig config(R"(
modbus:
  networks:
    - name: tcptest
      address: localhost
      port: 501
      slaves:
        - address: 1
          poll_groups:
            - register: 2
              register_type: holding
              count: 2
mqtt:
  client_id: mqtt_test
  refresh: 50ms
  broker:
    host: localhost
  objects:
    - topic: test_sensor
      retain: false
      state:
        register: tcptest.1.2
    - topic: other_sensor
      state:
        register: tcptest.1.3
)");

    SECTION("should not publish state if related register value is not changed") {
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 2);
        server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, 3);

        server.start();
        server.waitForPublish("other_sensor/state");

        //this caused test_sensor value change due to null -> 3
        server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, 33);
        server.waitForPublish("other_sensor/state");

        server.requirePublishCount("test_sensor/state", 1);
    }
}
