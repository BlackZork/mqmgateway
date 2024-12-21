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
  refresh: 100ms
  broker:
    host: localhost
  objects:
    - topic: test_sensor
      state:
        register: tcptest.1.2
)");

    SECTION("is set to false then initial poll triggers publish") {
        config.mYAML["mqtt"]["objects"][0]["retain"] = "false";
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 1);
        std::chrono::time_point<std::chrono::steady_clock> initialPollStart = std::chrono::steady_clock::now();
        server.start();
        server.waitForPublish("test_sensor/state");
        auto end = std::chrono::steady_clock::now();
        auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - initialPollStart).count();
        REQUIRE(dur <  20);
    }

    SECTION("is set to true then initial poll do not trigger publish") {
        config.mYAML["mqtt"]["objects"][0]["retain"] = "true";
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 1);
        server.start();
        server.waitForInitialPoll("tcptest");
        // wait some time for mqttclient thread to process initial poll messages
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        server.requirePublishCount("test_sensor/state", 0);

        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 2);
        server.waitForPublish("test_sensor/state");
        REQUIRE(server.mqttValue("test_sensor/state") == "2");
    }

    SECTION("is set to true and publish mode is every_poll then initial poll triggers publish") {
        config.mYAML["mqtt"]["refresh"] = "10ms";
        config.mYAML["mqtt"]["objects"][0]["retain"] = "true";
        config.mYAML["mqtt"]["objects"][0]["publish_mode"] = "every_poll";
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 2);
        server.start();
        //5 ms initail poll read, 10ms wait, 5ms second read, 5ms test tolerance
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
        server.stop();

        int test_count = server.mMqtt->getPublishCount("test_sensor/state");
        REQUIRE(test_count == 2);
    }

}

