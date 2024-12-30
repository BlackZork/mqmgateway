#include "catch2/catch_all.hpp"
#include "mockedserver.hpp"
#include "defaults.hpp"
#include "yaml_utils.hpp"

TEST_CASE ("When retain") {

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

    SECTION("is set to true then initial poll triggers publish") {
        config.mYAML["mqtt"]["refresh"] = "100ms";
        config.mYAML["mqtt"]["objects"][0]["retain"] = "true";
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 1);
        std::chrono::time_point<std::chrono::steady_clock> initialPollStart = std::chrono::steady_clock::now();
        server.start();
        server.waitForPublish("test_sensor/state");
        // ensure that this was the first poll
        auto end = std::chrono::steady_clock::now();
        auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - initialPollStart).count();
        REQUIRE(dur <  50);
    }

    SECTION("is set to false") {
        config.mYAML["mqtt"]["objects"][0]["retain"] = "false";

        SECTION("then initial poll triggers null value publish") {
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

        SECTION("and publish mode is every_poll then initial poll triggers publish") {
            config.mYAML["mqtt"]["refresh"] = "20ms";
            config.mYAML["mqtt"]["objects"][0]["publish_mode"] = "every_poll";
            MockedModMqttServerThread server(config.toString());
            server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 2);
            server.start();
            //5 ms initial poll read, 20ms wait, 5ms second read, 20ms test tolerance
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            server.stop();

            // retained messsage delete and two state publishes
            int test_count = server.mMqtt->getPublishCount("test_sensor/state");
            REQUIRE(test_count == 3);
        }

        SECTION("then availability change do not publish old value") {
            MockedModMqttServerThread server(config.toString());
            server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 1);
            server.start();

            server.waitForPublish("test_sensor/state");
            REQUIRE(server.mqttNullValue("test_sensor/state"));

            server.waitForPublish("test_sensor/availability");
            REQUIRE(server.mqttValue("test_sensor/availability") == "1");

            // do not publish 1 -> 2 change because availability is 0
            server.setModbusRegisterReadError("tcptest", 1, 2, modmqttd::RegisterType::HOLDING);
            server.waitForPublish("test_sensor/availability", defaultWaitTime() * 4);
            REQUIRE(server.mqttValue("test_sensor/availability") == "0");

            server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 2);
            server.clearModbusRegisterReadError("tcptest", 1, 2, modmqttd::RegisterType::HOLDING);
            // make sure that there is at least one sucessfull
            // poll that is not published
            std::this_thread::sleep_for(std::chrono::milliseconds(35));

            server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 3);

            server.waitForPublish("test_sensor/state");
            REQUIRE(server.mqttValue("test_sensor/state") == "3");
            // retain delete after inital poll and 2 -> 3 change only
            server.requirePublishCount("test_sensor/state", 2);
        }
    }
}

TEST_CASE ("When retain topic is a part of poll group") {

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

    SECTION("its value should be ignored if related register value is not changed") {
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
