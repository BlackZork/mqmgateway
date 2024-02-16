#include "catch2/catch_all.hpp"
#include "mockedserver.hpp"
#include "defaults.hpp"
#include "yaml_utils.hpp"


TEST_CASE("Silence before first poll") {

TestConfig config(R"(
modbus:
  networks:
    - name: tcptest
      address: localhost
      port: 501
      slaves:
        - address: 1
          delay_before_first_poll: 100ms
mqtt:
  client_id: mqtt_test
  broker:
    host: localhost
  objects:
    - topic: need_silence
      state:
        refresh: 5ms
        register: tcptest.1.1
)");

    SECTION("should be ignored if we poll the same slave") {
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 1, modmqttd::RegisterType::HOLDING, 1);
        server.start();
        // default mocked modbus read time is 5ms per register
        // refresh is 5ms so without silence poll should be executed every 10ms
        server.waitForPublish("need_silence/state");
        REQUIRE(server.mqttValue("need_silence/state") == "1");
        std::chrono::time_point<std::chrono::steady_clock> first_poll_ts = server.getLastPollTime();

        // we should not respect 100ms silence - slave not changed
        server.setModbusRegisterValue("tcptest", 1, 1, modmqttd::RegisterType::HOLDING, 2);
        server.waitForPublish("need_silence/state");
        auto ptime = server.getLastPollTime() - first_poll_ts;
        REQUIRE(ptime < std::chrono::milliseconds(100));
        REQUIRE(server.mqttValue("need_silence/state") == "2");

        server.stop();
    }
}

TEST_CASE("Silence before first poll should be respected if slave changes between polls") {

TestConfig config(R"(
modbus:
  networks:
    - name: tcptest
      address: localhost
      port: 501
      slaves:
        - address: 1
          delay_before_first_poll: 50ms
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
        refresh: 25ms
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
    // but afte initial poll last polled slave was from fast_one
    // so we want to acomodate available silence
    // and wait until we can poll need_silence

    server.setModbusRegisterValue("tcptest", 1, 1, modmqttd::RegisterType::HOLDING, 10);
    server.setModbusRegisterValue("tcptest", 2, 2, modmqttd::RegisterType::HOLDING, 200);

    // need silence was polled after 50ms,
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


