#include "catch2/catch_all.hpp"
#include "mockedserver.hpp"
#include "jsonutils.hpp"
#include "defaults.hpp"
#include "yaml_utils.hpp"
#include "testnumbers.hpp"

#ifdef HAVE_EXPRTK

TEST_CASE ("Exprtk plugin ") {

TestConfig config(R"(
modmqttd:
  converter_search_path:
    - build/exprconv
  converter_plugins:
    - exprconv.so
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
        count: 2
)");

SECTION ("should support uint32 helper") {
    config.mYAML["mqtt"]["objects"][0]["state"]["converter"] = "expr.evaluate('uint32(R0, R1)', precision=0)";
    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, TestNumbers::Int::AB);
    server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, TestNumbers::Int::CD);
    server.start();

    server.waitForPublish("test_state/state");

    REQUIRE(server.mqttValue("test_state/state") == std::to_string(TestNumbers::Int::ABCD_as_uint32));
    server.stop();
}

SECTION ("should support uint32bs helper") {
    config.mYAML["mqtt"]["objects"][0]["state"]["converter"] = "expr.evaluate('uint32bs(R0, R1)', precision=0)";
    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, TestNumbers::Int::BA);
    server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, TestNumbers::Int::DC);
    server.start();

    server.waitForPublish("test_state/state");

    REQUIRE(server.mqttValue("test_state/state") == std::to_string(TestNumbers::Int::ABCD_as_uint32));
    server.stop();
}

SECTION ("should support int32 helper") {
    config.mYAML["mqtt"]["objects"][0]["state"]["converter"] = "expr.evaluate('int32(R0, R1)', precision=0)";
    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, TestNumbers::Int::AB);
    server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, TestNumbers::Int::CD);
    server.start();

    server.waitForPublish("test_state/state");

    REQUIRE(server.mqttValue("test_state/state") == std::to_string(TestNumbers::Int::ABCD_as_int32));
    server.stop();
}

SECTION ("should support int32bs helper") {
    config.mYAML["mqtt"]["objects"][0]["state"]["converter"] = "expr.evaluate('int32bs(R0, R1)', precision=0)";
    MockedModMqttServerThread server(config.toString());
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, TestNumbers::Int::BA);
    server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, TestNumbers::Int::DC);
    server.start();

    server.waitForPublish("test_state/state");

    REQUIRE(server.mqttValue("test_state/state") == std::to_string(TestNumbers::Int::ABCD_as_int32));
    server.stop();
}



}

#endif
