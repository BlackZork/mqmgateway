#include "catch2/catch_all.hpp"
#include "mockedserver.hpp"
#include "defaults.hpp"
#include "yaml_utils.hpp"
#include <thread>

TEST_CASE("Scalar refresh: ") {

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
        register: tcptest.1.2
        register_type: input
)");

SECTION ("no_refresh_should_use_default_5s") {
    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 32456);
    server.start();
    server.waitForPublish("test_sensor/state");
    std::this_thread::sleep_for(std::chrono::milliseconds(5100));
    server.stop();
    REQUIRE(server.getMockedModbusContext("tcptest").getReadCount(1) == 2);
}

SECTION ("mqtt_refresh_should_override_default") {
    config.mYAML["mqtt"]["refresh"] = "80ms";
    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 32456);
    server.start();
    server.waitForPublish("test_sensor/state");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    server.stop();
    REQUIRE(server.getMockedModbusContext("tcptest").getReadCount(1) == 2);
}

SECTION ("topic_refresh_should_override_default") {
    config.mYAML["mqtt"]["objects"][0]["refresh"] = "80ms";
    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 32456);
    server.start();
    server.waitForPublish("test_sensor/state");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    server.stop();
    REQUIRE(server.getMockedModbusContext("tcptest").getReadCount(1) == 2);
}

SECTION ("topic_refresh_should_override_mqtt") {
    config.mYAML["mqtt"]["refresh"] = "10ms";
    config.mYAML["mqtt"]["objects"][0]["refresh"] = "80ms";
    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 32456);
    server.start();
    server.waitForPublish("test_sensor/state");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    server.stop();
    REQUIRE(server.getMockedModbusContext("tcptest").getReadCount(1) == 2);
}

SECTION ("state_refresh_should_override_topic") {
    config.mYAML["mqtt"]["objects"][0]["refresh"] = "10ms";
    config.mYAML["mqtt"]["objects"][0]["state"]["refresh"] = "80ms";
    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 32456);
    server.start();
    server.waitForPublish("test_sensor/state");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    server.stop();
    REQUIRE(server.getMockedModbusContext("tcptest").getReadCount(1) == 2);
}

SECTION ("state_refresh_should_override_mqtt") {
    config.mYAML["mqtt"]["refresh"] = "10ms";
    config.mYAML["mqtt"]["objects"][0]["state"]["refresh"] = "80ms";
    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 32456);
    server.start();
    server.waitForPublish("test_sensor/state");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    server.stop();
    REQUIRE(server.getMockedModbusContext("tcptest").getReadCount(1) == 2);
}

SECTION ("state_refresh_should_override_default") {
    config.mYAML["mqtt"]["objects"][0]["state"]["refresh"] = "80ms";
    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 32456);
    server.start();
    server.waitForPublish("test_sensor/state");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    server.stop();
    REQUIRE(server.getMockedModbusContext("tcptest").getReadCount(1) == 2);
}

} // TEST CASE

TEST_CASE("Register list refresh:") {

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
            register_type: input
          - register: tcptest.1.3
            register_type: input
)");

SECTION ("list_item_refresh_should_override_default") {
    config.mYAML["mqtt"]["objects"][0]["state"]["registers"][0]["refresh"] = "80ms";
    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 32456);
    server.start();
    server.waitForPublish("test_sensor/state");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    server.stop();
    REQUIRE(server.getMockedModbusContext("tcptest").getReadCount(1) == 3);
}

SECTION ("list_item_refresh_should_override_state") {
    config.mYAML["mqtt"]["objects"][0]["state"]["refresh"] = "10ms";
    config.mYAML["mqtt"]["objects"][0]["state"]["registers"][0]["refresh"] = "80ms";
    config.mYAML["mqtt"]["objects"][0]["state"]["registers"][1]["refresh"] = "1500ms";
    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 32456);
    server.start();
    server.waitForPublish("test_sensor/state");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    server.stop();
    REQUIRE(server.getMockedModbusContext("tcptest").getReadCount(1) == 3);
}

SECTION ("list_item_refresh_should_override_state") {
    config.mYAML["mqtt"]["objects"][0]["refresh"] = "10ms";
    config.mYAML["mqtt"]["objects"][0]["state"]["registers"][0]["refresh"] = "80ms";
    config.mYAML["mqtt"]["objects"][0]["state"]["registers"][1]["refresh"] = "1500ms";
    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 32456);
    server.start();
    server.waitForPublish("test_sensor/state");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    server.stop();
    REQUIRE(server.getMockedModbusContext("tcptest").getReadCount(1) == 3);
}


SECTION ("list_item_refresh_should_override_default") {
    config.mYAML["mqtt"]["objects"][0]["state"]["registers"][0]["refresh"] = "80ms";
    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 32456);
    server.start();
    server.waitForPublish("test_sensor/state");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    server.stop();
    REQUIRE(server.getMockedModbusContext("tcptest").getReadCount(1) == 3);
}


};
