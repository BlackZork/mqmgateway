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

TEST_CASE("Silence before poll should not reorder register polling") {

TestConfig config(R"(
modbus:
  networks:
    - name: tcptest
      address: localhost
      port: 501
      slaves:
        - address: 1
          silence_before_poll: 50ms
mqtt:
  client_id: mqtt_test
  broker:
    host: localhost
  objects:
    - topic: need_silence
      state:
        refresh: 5ms
        register: tcptest.1.1
      availablilty: false
    - topic: fast_one
      state:
        refresh: 10ms
        register: tcptest.2.2
      availablilty: false
)");

    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 1, modmqttd::RegisterType::HOLDING, 1);
    server.setModbusRegisterValue("tcptest", 2, 2, modmqttd::RegisterType::HOLDING, 20);
    server.start();

    //wait for initial poll
    server.waitForPublish("need_silence/state");
    REQUIRE(server.mqttValue("need_silence/state") == "1");
    server.waitForPublish("fast_one/state");
    REQUIRE(server.mqttValue("fast_one/state") == "20");
    std::chrono::time_point<std::chrono::steady_clock> first_poll_ts = server.getLastPollTime();

    // next to refresh is need_silence
    // so fast_one need to wait 50ms to be polled next
    // both registers should be polled at the same time:
    // need_silence after 50ms due to silece_before_poll
    // and fast_one just after need_silence, it is polled later than declared 10ms

    server.setModbusRegisterValue("tcptest", 1, 1, modmqttd::RegisterType::HOLDING, 10);
    server.setModbusRegisterValue("tcptest", 2, 2, modmqttd::RegisterType::HOLDING, 200);

    // wait for fast_one
    server.waitForPublish("fast_one/state");
    auto stime = server.getLastPollTime();
    auto ptime = stime - first_poll_ts;
    //5ms poll time + 50ms silence
    REQUIRE(ptime > std::chrono::milliseconds(50));

    // need_silence was polled before fast_one
    // check last published mqtt value
    auto sval = server.mqttValue("need_silence/state");
    REQUIRE(sval == "10");

    server.stop();
}
