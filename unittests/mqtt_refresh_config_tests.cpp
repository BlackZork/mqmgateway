#include "catch2/catch_all.hpp"
#include "mockedserver.hpp"
#include "yaml_utils.hpp"
#include <thread>

TEST_CASE("Refresh config: ") {

TestConfig config(R"(
modbus:
  networks:
    - name: tcptest
      address: localhost
      port: 501
mqtt:
  client_id: mqtt_test
  broker:
    host: localhost
  objects:
    - topic: test_sensor
      state:
        registers:
          - register: tcptest.1.2
          - register: tcptest.1.3
)");

SECTION ("no refresh should use default 5s") {
    MockedModMqttServerThread server(config.toString());
    server.start();
    // the first publish tells us that the poll specification has crossed the
    // queue and reached the scheduler
    server.waitForPublish("test_sensor/state");

    // The default is a modmqttd constant that never reaches the yaml, so
    // TestConfig cannot scale it and it stays 5s at every timing factor.
    const auto& spec = server.getServer().getModbusClient("tcptest").getThread().getScheduler().getPollSpecification();
    const auto& slave1 = spec.at(1);
    REQUIRE(slave1.size() == 2);
    for (const auto& reg: slave1)
        REQUIRE(reg->mRefresh == std::chrono::seconds(5));

    server.stop();
}

SECTION ("mqtt refresh should override default") {
    config.mYAML["mqtt"]["refresh"] = "80ms";
    MockedModMqttServerThread server(config.toString());
    server.start();
    server.waitForPublish("test_sensor/state");
    std::this_thread::sleep_for(timing::milliseconds(100));
    server.stop();
    REQUIRE(server.getMockedModbusContext("tcptest").getIssuedReadCallsCount(1) == 4);
}

SECTION ("topic refresh should override default") {
    config.mYAML["mqtt"]["objects"][0]["refresh"] = "80ms";
    MockedModMqttServerThread server(config.toString());
    server.start();
    server.waitForPublish("test_sensor/state");
    std::this_thread::sleep_for(timing::milliseconds(100));
    server.stop();
    REQUIRE(server.getMockedModbusContext("tcptest").getIssuedReadCallsCount(1) == 4);
}

SECTION ("topic refresh should override_mqtt") {
    config.mYAML["mqtt"]["refresh"] = "10ms";
    config.mYAML["mqtt"]["objects"][0]["refresh"] = "1s";
    config.mYAML["mqtt"]["objects"][0]["refresh"] = "80ms";
    MockedModMqttServerThread server(config.toString());
    server.start();
    server.waitForPublish("test_sensor/state");
    std::this_thread::sleep_for(timing::milliseconds(100));
    server.stop();
    REQUIRE(server.getMockedModbusContext("tcptest").getIssuedReadCallsCount(1) == 4);
}

SECTION ("state refresh should override_topic") {
    config.mYAML["mqtt"]["objects"][0]["refresh"] = "10ms";
    config.mYAML["mqtt"]["objects"][0]["state"]["refresh"] = "80ms";
    MockedModMqttServerThread server(config.toString());
    server.start();
    server.waitForPublish("test_sensor/state");
    std::this_thread::sleep_for(timing::milliseconds(100));
    server.stop();
    REQUIRE(server.getMockedModbusContext("tcptest").getIssuedReadCallsCount(1) == 4);
}

SECTION ("state refresh should override_mqtt") {
    config.mYAML["mqtt"]["refresh"] = "10ms";
    config.mYAML["mqtt"]["objects"][0]["state"]["refresh"] = "80ms";
    MockedModMqttServerThread server(config.toString());
    server.start();
    server.waitForPublish("test_sensor/state");
    std::this_thread::sleep_for(timing::milliseconds(100));
    server.stop();
    REQUIRE(server.getMockedModbusContext("tcptest").getIssuedReadCallsCount(1) == 4);
}

SECTION ("state refresh should override default") {
    config.mYAML["mqtt"]["objects"][0]["state"]["refresh"] = "80ms";
    MockedModMqttServerThread server(config.toString());
    server.start();
    server.waitForPublish("test_sensor/state");
    std::this_thread::sleep_for(timing::milliseconds(100));
    server.stop();
    REQUIRE(server.getMockedModbusContext("tcptest").getIssuedReadCallsCount(1) == 4);
}

SECTION ("list item refresh should override default") {
    config.mYAML["mqtt"]["objects"][0]["state"]["registers"][0]["refresh"] = "80ms";
    MockedModMqttServerThread server(config.toString());
    server.start();
    server.waitForPublish("test_sensor/state");
    std::this_thread::sleep_for(timing::milliseconds(100));
    server.stop();
    REQUIRE(server.getMockedModbusContext("tcptest").getIssuedReadCallsCount(1) == 3);
}

SECTION ("list item refresh should override state") {
    config.mYAML["mqtt"]["objects"][0]["state"]["refresh"] = "10ms";
    config.mYAML["mqtt"]["objects"][0]["state"]["registers"][0]["refresh"] = "80ms";
    config.mYAML["mqtt"]["objects"][0]["state"]["registers"][1]["refresh"] = "1500ms";
    MockedModMqttServerThread server(config.toString());
    server.start();
    server.waitForPublish("test_sensor/state");
    std::this_thread::sleep_for(timing::milliseconds(100));
    server.stop();
    REQUIRE(server.getMockedModbusContext("tcptest").getIssuedReadCallsCount(1) == 3);
}

SECTION ("list item refresh should override topic") {
    config.mYAML["mqtt"]["objects"][0]["refresh"] = "10ms";
    config.mYAML["mqtt"]["objects"][0]["state"]["registers"][0]["refresh"] = "80ms";
    config.mYAML["mqtt"]["objects"][0]["state"]["registers"][1]["refresh"] = "1500ms";
    MockedModMqttServerThread server(config.toString());
    server.start();
    server.waitForPublish("test_sensor/state");
    std::this_thread::sleep_for(timing::milliseconds(100));
    server.stop();
    REQUIRE(server.getMockedModbusContext("tcptest").getIssuedReadCallsCount(1) == 3);
}


SECTION ("list item refresh should override default") {
    config.mYAML["mqtt"]["objects"][0]["state"]["registers"][0]["refresh"] = "80ms";
    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 32456);
    server.start();
    server.waitForPublish("test_sensor/state");
    std::this_thread::sleep_for(timing::milliseconds(100));
    server.stop();
    REQUIRE(server.getMockedModbusContext("tcptest").getIssuedReadCallsCount(1) == 3);
}

} // TEST CASE

