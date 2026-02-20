#include "catch2/catch_all.hpp"
#include "mockedserver.hpp"
#include "yaml_utils.hpp"


TEST_CASE("Silence before first poll set for single slave") {

TestConfig config(R"(
modbus:
  networks:
    - name: tcptest
      address: localhost
      port: 501
      slaves:
        - address: 1
          delay_before_first_command: 100ms
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
        REQUIRE(ptime < timing::milliseconds(100));
        REQUIRE(server.mqttValue("need_silence/state") == "2");

        server.stop();
    }
}

TEST_CASE("Silence before first poll") {

TestConfig config(R"(
modbus:
  networks:
    - name: tcptest
      address: localhost
      port: 501
      slaves:
        - address: 1
          delay_before_first_command: 50ms
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

SECTION("should be respected if slave changes between polls") {

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
    // but after initial poll last polled slave was from fast_one
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
    REQUIRE(ptime > timing::milliseconds(50));

    // need_silence was polled before fast_one
    // check last published mqtt value
    auto sval = server.mqttValue("need_silence/state");
    REQUIRE(sval == "10");

    server.stop();
};

SECTION("should be respected if delay_before_command is set") {
    config.mYAML["modbus"]["networks"][0]["slaves"][0]["delay_before_command"] = "25ms";
    config.mYAML["mqtt"]["objects"][1]["state"]["refresh"] = "150ms";

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
    // but after initial poll last polled slave was from fast_one
    // so we want to acomodate available silence
    // and wait until we can poll need_silence

    server.setModbusRegisterValue("tcptest", 1, 1, modmqttd::RegisterType::HOLDING, 10);
    server.setModbusRegisterValue("tcptest", 2, 2, modmqttd::RegisterType::HOLDING, 200);

    // need_silence was polled after 50ms,
    server.waitForPublish("need_silence/state");
    auto stime = server.getLastPollTime();
    auto ptime = stime - first_poll_ts;
    //5ms poll time + 50ms silence
    //but not 100ms from delay_before_command
    REQUIRE(ptime > timing::milliseconds(50));
    server.setModbusRegisterValue("tcptest", 1, 1, modmqttd::RegisterType::HOLDING, 20);

    // need_silence was polled again, but after 25 ms
    // due to delay_before_command set

    server.waitForPublish("need_silence/state");
    ptime = server.getLastPollTime() - stime;
    //5ms poll time + 25ms silence
    REQUIRE(ptime > timing::milliseconds(25));
    REQUIRE(ptime < timing::milliseconds(50));

    server.stop();
}

}


