#include "catch2/catch_all.hpp"
#include "mockedserver.hpp"
#include "defaults.hpp"
#include "yaml_utils.hpp"


TEST_CASE("Silence before poll") {

TestConfig config(R"(
modbus:
  networks:
    - name: tcptest
      address: localhost
      port: 501
      slaves:
        - address: 1
          silence_before_poll: 15ms
mqtt:
  client_id: mqtt_test
  broker:
    host: localhost
  objects:
    - topic: test_sensor
      state:
        refresh: 5ms
        register: tcptest.1.2
)");

    SECTION("should be respected even when next poll is eariler") {
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 1);
        server.start();
        // default mocked modbus read time is 5ms per register
        // refresh is 5ms sowithout silence poll should be executed every 10ms
        server.waitForPublish("test_sensor/state");
        REQUIRE(server.mqttValue("test_sensor/state") == "1");
        std::chrono::time_point<std::chrono::steady_clock> first_poll_ts = server.getLastPollTime();

        // we should respect 15ms silence
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 2);
        server.waitForPublish("test_sensor/state");
        auto ptime = server.getLastPollTime() - first_poll_ts;
        REQUIRE(ptime > std::chrono::milliseconds(15));
        REQUIRE(server.mqttValue("test_sensor/state") == "2");

        server.stop();
    }

    SECTION("should be ignored when next poll is later") {
        config.mYAML["mqtt"]["objects"][0]["state"]["refresh"] = "25ms";
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 1);
        server.start();
        server.waitForPublish("test_sensor/state");
        REQUIRE(server.mqttValue("test_sensor/state") == "1");
        std::chrono::time_point<std::chrono::steady_clock> first_poll_ts = server.getLastPollTime();

        // we should poll in 25ms, ignoring 15ms silence period
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 2);
        server.waitForPublish("test_sensor/state");
        auto ptime = server.getLastPollTime() - first_poll_ts;
        // add 5ms for poll time, 5ms for code execution
        REQUIRE(ptime < std::chrono::milliseconds(35));
        REQUIRE(server.mqttValue("test_sensor/state") == "2");

        server.stop();
    }

}

