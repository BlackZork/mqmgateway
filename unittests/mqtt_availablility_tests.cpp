#include "catch2/catch_all.hpp"

#include "defaults.hpp"
#include "jsonutils.hpp"
#include "mockedserver.hpp"
#include "yaml_utils.hpp"


TEST_CASE ("Availability from multiple registers") {

TestConfig config(R"(
modmqttd:
  converter_search_path:
    - build/stdconv
  converter_plugins:
    - stdconv.so
modbus:
  networks:
    - name: tcptest
      address: localhost
      port: 501
mqtt:
  client_id: mqtt_test
  refresh: 1s
  broker:
    host: localhost
  objects:
    - topic: test_state
      state:
        register: tcptest.1.2
      availability:
        converter: std.int32()
        available_value: 65537
)");

SECTION ("should be set after conversion from register and count") {
    config.mYAML["mqtt"]["objects"][0]["availability"]["register"] = "tcptest.1.2";
    config.mYAML["mqtt"]["objects"][0]["availability"]["count"] = "2";

    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 1);
    server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, 1);
    server.start();

    //to make sure that all registers have initial value
    server.waitForPublish("test_state/availability");
    REQUIRE(server.mqttValue("test_state/availability") == "1");

    server.waitForPublish("test_state/state");

    REQUIRE(server.mModbusFactory->getMockedModbusContext("tcptest").getReadCount(1) == 1);
    REQUIRE_JSON(server.mqttValue("test_state/state"), "1");
    server.stop();
}

SECTION ("should be set after conversion from register list") {
    config.mYAML["mqtt"]["objects"][0]["availability"]["registers"].push_back(YAML::Node(YAML::NodeType::Map));
    config.mYAML["mqtt"]["objects"][0]["availability"]["registers"].push_back(YAML::Node(YAML::NodeType::Map));


    config.mYAML["mqtt"]["objects"][0]["availability"]["registers"][0]["register"] = "tcptest.1.2";
    config.mYAML["mqtt"]["objects"][0]["availability"]["registers"][1]["register"] = "tcptest.1.3";

    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 1);
    server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, 1);
    server.start();

    //to make sure that all registers have initial value
    server.waitForPublish("test_state/availability");
    REQUIRE(server.mqttValue("test_state/availability") == "1");

    server.waitForPublish("test_state/state");

    REQUIRE_JSON(server.mqttValue("test_state/state"), "1");
    server.stop();
}


}
