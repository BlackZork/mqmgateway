#include "catch2/catch_all.hpp"

#include "defaults.hpp"
#include "mockedserver.hpp"
#include "yaml_utils.hpp"

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

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
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
      device: /tmp/watchdog_test
      baud: 9600
      parity: E
      data_bit: 8
      stop_bit: 1
      watchdog:
        watch_period: 10s
mqtt:
  client_id: mqtt_test
  refresh: 100ms
  broker:
    host: localhost
  objects:
    - topic: slave1
      state:
        register: rtutest.1.1
    - topic: slave2
      state:
        register: rtutest.2.2
)";

    SECTION("should close usb serial port if unplugged") {
        MockedModMqttServerThread server(config);
        server.start();

        server.waitForPublish("slave2/availability");
        REQUIRE(server.mqttValue("slave2/availability") == "1");

        //simulate USB unplug
        server.disconnectSerialPortFor("rtutest");
        server.disconnectModbusSlave("rtutest", 1);
        server.disconnectModbusSlave("rtutest", 2);

        std::this_thread::sleep_for(std::chrono::seconds(2));
        server.stop();

        //at least one attempt to reconnect
        REQUIRE(server.getMockedModbusContext("rtutest").getConnectionCount() >= 2);
    }


}

SECTION("with a long poll time") {
static const std::string config = R"(
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
    - topic: slave1
      state:
        register: tcptest.1.1
        refresh: 30s
    - topic: slave2
      state:
        register: tcptest.2.2
        refresh: 10s
)";

    SECTION("should sets its duration two times longer than minimum refresh") {
        MockedModMqttServerThread server(config);
        server.start();
        server.stop();

        //one or two attempts to reconnect
        REQUIRE(server.getServer().getModbusClient("tcptest").getThread().getWatchdog().getConfig().mWatchPeriod == std::chrono::seconds(20));
    }
}

TestConfig config(R"(
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
)");


    SECTION("should set availability flag if slave is available after reconnect") {
        MockedModMqttServerThread server(config.toString());
        server.start();

        server.waitForPublish("slave1/availability");
        REQUIRE(server.mqttValue("slave1/availability") == "1");

        server.disconnectModbusSlave("tcptest", 1);

        server.waitForPublish("slave1/availability", std::chrono::milliseconds(1000));
        REQUIRE(server.mqttValue("slave1/availability") == "0");

        server.connectModbusSlave("tcptest", 1);
        server.getMockedModbusContext("tcptest").waitForInitialPoll(std::chrono::milliseconds(1000));

        std::string topic = server.waitForFirstPublish();
        REQUIRE(topic == "slave1/state");

        server.waitForPublish("slave1/availability");
        REQUIRE(server.mqttValue("slave1/availability") == "1");

        server.stop();
    }

    SECTION("should not set availability flag if slave is not available after reconnect") {
        MockedModMqttServerThread server(config.toString());
        server.start();

        server.waitForPublish("slave1/availability");
        REQUIRE(server.mqttValue("slave1/availability") == "1");

        server.disconnectModbusSlave("tcptest", 1);

        server.waitForPublish("slave1/availability", std::chrono::milliseconds(1000));
        REQUIRE(server.mqttValue("slave1/availability") == "0");

        server.getMockedModbusContext("tcptest").waitForInitialPoll();

        // make sure that all changes are published after inital poll
        std::this_thread::sleep_for(std::chrono::seconds(1));
        REQUIRE(server.mqttValue("slave1/availability") == "0");

        server.stop();
    }


    SECTION("should not read modbus registers after reconnect if publish_mode is set to once") {
        config.mYAML["mqtt"]["publish_mode"] = "once";
        MockedModMqttServerThread server(config.toString());
        server.start();

        server.waitForPublish("slave1/availability");
        REQUIRE(server.mqttValue("slave1/availability") == "1");

        server.disconnectModbusSlave("tcptest", 1);

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        server.getMockedModbusContext("tcptest").waitForInitialPoll();

        // slave is unavailable, but register was read
        // once so its value is available until modmqttd is restarted
        REQUIRE(server.mqttValue("slave1/availability") == "1");

        server.stop();
        REQUIRE(server.getMockedModbusContext("tcptest").getReadCount(1) == 1);
    }


} //CASE
