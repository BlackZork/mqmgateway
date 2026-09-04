#include <catch2/catch_all.hpp>

#include "mockedserver.hpp"
#include "yaml_utils.hpp"

/**
 * broken_sensor attaches std.float32 to a single register. A 32-bit float is
 * assembled from two registers, so FloatConverter::toMqtt throws ConvException
 * when it gets only one - every poll of that object fails to convert. The
 * register value plays no part in it, the register count alone makes it throw.
 *
 * working_sensor is a plain uint16 on a separate register. The test asserts
 * that broken_sensor failing does not stop working_sensor from being published,
 * and that polling keeps running afterwards.
 */
static const std::string config_str = R"(
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
  refresh: 100ms
  broker:
    host: localhost
  objects:
    - topic: broken_sensor
      state:
        register: tcptest.1.2
        register_type: input
        converter: std.float32()
    - topic: working_sensor
      state:
        register: tcptest.1.3
        register_type: input
)";

TEST_CASE("A converter failing to convert polled data") {
    TestConfig config(config_str);

    SECTION("should not stop other objects from being published") {
        MockedModMqttServerThread server(config.toString());
        // any value does, std.float32 rejects this register on its count alone
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, 0x1234);
        server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::INPUT, 5);
        server.start();

        server.waitForPublish("working_sensor/state");
        REQUIRE(server.mqttValue("working_sensor/state") == "5");

        // a new value proves that polling continued after the failed conversion
        server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::INPUT, 6);
        server.waitForMqttValue("working_sensor/state", "6");

        // stop() checks that no exception escaped the server thread
        server.stop();
    }
}
