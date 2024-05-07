#include "catch2/catch_all.hpp"

#include "defaults.hpp"
#include "mockedserver.hpp"

TEST_CASE ("Modbus watchdog") {

SECTION("on TCP network") {

static const std::string config = R"(
modbus:
  networks:
    - name: tcptest
      address: localhost
      port: 501
      watchdog:
        watch_period: 300ms
mqtt:
  client_id: mqtt_test
  refresh: 100ms
  broker:
    host: localhost
  objects:
    - topic: slave1
      state:
        register: tcptest.1.1
    - topic: slave2
      state:
        register: tcptest.2.2
)";


    SECTION("should restart connection if no command can be executed") {
        MockedModMqttServerThread server(config);
        server.start();

        server.waitForPublish("slave2/availability");
        REQUIRE(server.mqttValue("slave2/availability") == "1");

        server.disconnectModbusSlave("tcptest", 1);
        server.disconnectModbusSlave("tcptest", 2);

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        server.stop();

        //one or two attempts to reconnect
        REQUIRE(server.getMockedModbusContext("tcptest").getConnectionCount() >= 2);
        REQUIRE(server.getMockedModbusContext("tcptest").getConnectionCount() <= 3);
    }

    SECTION("should not restart connection if at least one slave is alive") {
        MockedModMqttServerThread server(config);
        server.start();

        server.waitForPublish("slave2/availability");
        REQUIRE(server.mqttValue("slave2/availability") == "1");

        server.disconnectModbusSlave("tcptest", 1);

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        server.stop();

        //no reconnections, slave1 was alive all the time
        REQUIRE(server.getMockedModbusContext("tcptest").getConnectionCount() == 1);
    }

}

SECTION("on RTU network") {
static const std::string config = R"(
modbus:
  networks:
    - name: rtutest
      device: /dev/ttyUSB0
      baud: 9600
      parity: E
      data_bit: 8
      stop_bit: 1
      watchdog:
        watch_period: 1s
mqtt:
  client_id: mqtt_test
  refresh: 100ms
  broker:
    host: localhost
  objects:
    - topic: slave1
      state:
        register: tcptest.1.1
    - topic: slave2
      state:
        register: tcptest.2.2
)";

    SECTION("should close usb serial port if unplugged") {
        MockedModMqttServerThread server(config);
        server.start();

        server.waitForPublish("slave2/availability");
        REQUIRE(server.mqttValue("slave2/availability") == "1");

        server.disconnectSerialPortFor("rtutest");

        std::this_thread::sleep_for(std::chrono::seconds(2));
        server.stop();

//        REQUIRE(server.getMockedModbusContext("rtutest").getWatchdog().getFailedSerialPortCheckCount() > 0);
        //at least one attempt to reconnect
        REQUIRE(server.getMockedModbusContext("rtutest").getConnectionCount() >= 2);
    }


}

} //CASE
