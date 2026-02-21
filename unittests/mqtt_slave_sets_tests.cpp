#include "catch2/catch_all.hpp"
#include "mockedserver.hpp"
#include "yaml_utils.hpp"

TEST_CASE ("Slave set config") {

TestConfig config(R"(
modbus:
  networks:
    - name: tcptest
      address: localhost
      port: 502
mqtt:
  client_id: mqtt_test
  broker:
    host: localhost
  objects:
    - topic: slave_${slave_address}/test_sensor
      network: tcptest
      slave: 1, 2-3
      state:
        register: 1
)");

SECTION("should publish topics with address placeholder for every slave") {
    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 1, modmqttd::RegisterType::HOLDING, 1);
    server.setModbusRegisterValue("tcptest", 2, 1, modmqttd::RegisterType::HOLDING, 2);
    server.setModbusRegisterValue("tcptest", 3, 1, modmqttd::RegisterType::HOLDING, 3);

    server.start();
    server.waitForPublish("slave_1/test_sensor/state");
    server.waitForPublish("slave_2/test_sensor/state");
    server.waitForPublish("slave_3/test_sensor/state");

    REQUIRE(server.mqttValue("slave_1/test_sensor/state") == "1");
    REQUIRE(server.mqttValue("slave_2/test_sensor/state") == "2");
    REQUIRE(server.mqttValue("slave_3/test_sensor/state") == "3");
    server.stop();
}

SECTION("should throw ConfigurationException if slave address placeholder is missing") {
    config.mYAML["mqtt"]["objects"][0]["topic"] = "slave/test_sensor";
    MockedModMqttServerThread server(config.toString(), false);
    server.start();
    server.stop();
    REQUIRE(server.initOk() == false);
}

SECTION("should throw ConfigurationException if slave address is duplicated") {
    config.mYAML["mqtt"]["objects"][0]["slave"] = "1-3,3";
    MockedModMqttServerThread server(config.toString(), false);
    server.start();
    server.stop();
    REQUIRE(server.initOk() == false);
}

SECTION("should throw ConfigurationException if slave address range is invalid") {
    config.mYAML["mqtt"]["objects"][0]["slave"] = "5-1";
    MockedModMqttServerThread server(config.toString(), false);
    server.start();
    server.stop();
    REQUIRE(server.initOk() == false);
}


}

TEST_CASE("Named slave set config") {

TestConfig config(R"(
modbus:
  networks:
    - name: tcptest
      address: localhost
      port: 502
      slaves:
        - address: 1
          name: meter
        - address: 2
mqtt:
  client_id: mqtt_test
  broker:
    host: localhost
  objects:
    - topic: slave_${slave_name}/test_sensor
      network: tcptest
      slave: 1-2
      state:
        register: 1
)");

SECTION("should throw ConfigurationException if no slave name is set and slave_name placeholder is used") {
    config.mYAML["modbus"]["networks"][0].remove("slaves");
    MockedModMqttServerThread server(config.toString(), false);
    server.start();
    server.stop();
    REQUIRE(server.initOk() == false);
}

SECTION("should throw ConfigurationException slave is not defined in topic section") {
    config.mYAML["modbus"]["networks"][0]["slaves"][1]["name"] = "switch";
    config.mYAML["mqtt"]["objects"][0].remove("slave");
    config.mYAML["mqtt"]["objects"][0]["state"]["register"] = 1.1;
    MockedModMqttServerThread server(config.toString(), false);
    server.start();
    server.stop();
    REQUIRE(server.initOk() == false);
}


SECTION("should throw ConfigurationException if not all slaves have a name") {
    MockedModMqttServerThread server(config.toString(), false);
    server.start();
    server.stop();
    REQUIRE(server.initOk() == false);
}

SECTION("should publish topics with name placeholder for every slave") {
    config.mYAML["modbus"]["networks"][0]["slaves"][1]["name"] = "switch";
    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 1, modmqttd::RegisterType::HOLDING, 1);
    server.setModbusRegisterValue("tcptest", 2, 1, modmqttd::RegisterType::HOLDING, 2);

    server.start();
    server.waitForPublish("slave_meter/test_sensor/state");
    server.waitForPublish("slave_switch/test_sensor/state");

    REQUIRE(server.mqttValue("slave_meter/test_sensor/state") == "1");
    REQUIRE(server.mqttValue("slave_switch/test_sensor/state") == "2");
    server.stop();
}

}

TEST_CASE ("Network set config") {

TestConfig config(R"(
modbus:
  networks:
    - name: local
      address: localhost
      port: 502
    - name: other
      address: otherhost
      port: 502
mqtt:
  client_id: mqtt_test
  broker:
    host: localhost
  objects:
    - topic: net_${network}/test_sensor
      network: local, other
      state:
        register: 1.1
)");

SECTION("should publish topics with network placeholder for every network") {

    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("local", 1, 1, modmqttd::RegisterType::HOLDING, 1);
    server.setModbusRegisterValue("other", 1, 1, modmqttd::RegisterType::HOLDING, 2);
    server.start();
    server.waitForPublish("net_local/test_sensor/state");
    server.waitForPublish("net_other/test_sensor/state");
    REQUIRE(server.mqttValue("net_local/test_sensor/state") == "1");
    REQUIRE(server.mqttValue("net_other/test_sensor/state") == "2");
    server.stop();
}


}
