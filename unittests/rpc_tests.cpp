#include <catch2/catch_all.hpp>
#include "mockedserver.hpp"
#include "yaml_utils.hpp"
#include "testnumbers.hpp"
#include <thread>

TEST_CASE("RPC mode subscription") {

    TestConfig config(R"(
modbus:
  networks:
    - name: tcptest
      address: localhost
      port: 501
      slaves:
        - address: 1
mqtt:
  client_id: mqtt_test
  broker:
    host: localhost
  objects: []
)");

    SECTION("should subscribe when mode is read") {
        config.mYAML["mqtt"]["rpc"]["mode"] = "read";
        MockedModMqttServerThread server(config.toString());
        server.start();
        server.waitForSubscription("mqtt_test/rpc/modbus_request");
        server.stop();
    }

    SECTION("should subscribe when mode is readwrite") {
        config.mYAML["mqtt"]["rpc"]["mode"] = "readwrite";
        MockedModMqttServerThread server(config.toString());
        server.start();
        server.waitForSubscription("mqtt_test/rpc/modbus_request");
        server.stop();
    }

    SECTION("should not subscribe when mode is disabled") {
        MockedModMqttServerThread server(config.toString());
        server.start();
        REQUIRE(!server.mMqtt->waitForSubscription("mqtt_test/rpc/modbus_request", timing::milliseconds(100)));
        server.stop();
    }
}


TEST_CASE("RPC read request") {

    TestConfig config(R"(
modbus:
  networks:
    - name: tcptest
      address: localhost
      port: 501
      slaves:
        - address: 1
          delay_before_command: 50ms
mqtt:
  client_id: mqtt_test
  rpc:
    mode: read
  broker:
    host: localhost
  objects: []
)");

    SECTION("single register should return scalar value") {
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 42);
        server.start();
        server.waitForSubscription("mqtt_test/rpc/modbus_request");

        const std::string req = R"({"network":"tcptest","slave":1,"register":"2"})";
        server.mMqtt->injectRpcRequest(
            "mqtt_test/rpc/modbus_request",
            req.c_str(), static_cast<int>(req.size()),
            "test/response", 1);

        server.waitForRpcResponse(1);
        REQUIRE(server.mMqtt->rpcValue(1) == "42");
        REQUIRE(server.mMqtt->rpcUserProperty(1, "error").empty());
        server.stop();
    }

    SECTION("multiple registers should return JSON array") {
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 10);
        server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, 20);
        server.start();
        server.waitForSubscription("mqtt_test/rpc/modbus_request");

        const std::string req = R"({"network":"tcptest","slave":1,"register":"2","count":2})";
        server.mMqtt->injectRpcRequest(
            "mqtt_test/rpc/modbus_request",
            req.c_str(), static_cast<int>(req.size()),
            "test/response", 1);

        server.waitForRpcResponse(1);
        REQUIRE(server.mMqtt->rpcValue(1) == "[10,20]");
        REQUIRE(server.mMqtt->rpcUserProperty(1, "error").empty());
        server.stop();
    }

    SECTION("without response topic should be dropped") {
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 0);
        server.start();
        server.waitForSubscription("mqtt_test/rpc/modbus_request");

        const std::string req = R"({"network":"tcptest","slave":1,"register":"2"})";
        server.mMqtt->injectRpcRequest(
            "mqtt_test/rpc/modbus_request",
            req.c_str(), static_cast<int>(req.size()),
            nullptr);

        REQUIRE(!server.mMqtt->waitForRpcResponse(0, timing::milliseconds(200)));
        server.stop();
    }
}


TEST_CASE("RPC read vs scheduled poll") {

    // register 1.2 is polled every 100ms. A burst of 30 RPC reads is fired at a
    // 20ms cadence - five times faster than the refresh - over ~600ms (~6 refresh
    // windows). When the RPC range fully covers the poll, every refresh window
    // contains several RPC reads that defer the poll, so the scheduler issues only
    // the initial poll. When the RPC does not fully cover the poll, the poll keeps
    // running on its own cadence regardless of the RPC reads.
    //
    // The assertion counts only scheduled-poll reads (commandId == 0); RPC reads
    // carry a negative commandId and are excluded, so the result is independent of
    // how many RPC reads completed and is not fragile under CPU load.
    TestConfig config(R"(
modbus:
  networks:
    - name: tcptest
      address: localhost
      port: 501
      slaves:
        - address: 1
mqtt:
  client_id: mqtt_test
  rpc:
    mode: read
  broker:
    host: localhost
  objects:
    - topic: test_state
      publish_mode: every_poll
      state:
        register: tcptest.1.2
        refresh: 100ms
        register_type: holding
)");

    // fires 30 RPC reads of pReq at a 20ms cadence, then returns how many scheduled
    // polls (commandId == 0) the slave actually ran
    auto scheduledPollReads = [](MockedModMqttServerThread& pServer, const std::string& pReq) -> int {
        pServer.start();
        pServer.waitForSubscription("mqtt_test/rpc/modbus_request");
        for (int i = 1; i <= 30; i++) {
            pServer.mMqtt->injectRpcRequest(
                "mqtt_test/rpc/modbus_request",
                pReq.c_str(), static_cast<int>(pReq.size()),
                "test/response", i);
            std::this_thread::sleep_for(timing::milliseconds(20));
        }
        for (int i = 1; i <= 30; i++) {
            pServer.waitForRpcResponse(i);
        }
        pServer.stop();

        auto& ctx = pServer.getMockedModbusContext("tcptest");
        int total = ctx.getIssuedReadCallsCount(1);
        int polls = 0;
        for (int i = 0; i < total; i++) {
            if (ctx.getIssuedReadCall(1, i).getCommandId() == 0) {
                polls++;
            }
        }
        return polls;
    };

    SECTION("with an exactly matching range defers the poll") {
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 42);

        const std::string req = R"({"network":"tcptest","slave":1,"register":"2"})";
        REQUIRE(scheduledPollReads(server, req) <= 2);
        REQUIRE(server.mMqtt->rpcValue(1) == "42");
        // the scheduled poll's modbus reads are deduplicated, but the state topic
        // keeps publishing at the poll cadence (~6 over ~600ms); the 30 fast RPC
        // reads neither flood it (>> 6) nor starve it (~1)
        REQUIRE(server.getPublishCount("test_state/state") >= 4);
        REQUIRE(server.getPublishCount("test_state/state") <= 12);
    }

    SECTION("with a wider range defers the narrower poll") {
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 42);
        server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, 43);

        const std::string req = R"({"network":"tcptest","slave":1,"register":"2","count":2})";
        REQUIRE(scheduledPollReads(server, req) <= 2);
        REQUIRE(server.mMqtt->rpcValue(1) == "[42,43]");
        REQUIRE(server.getPublishCount("test_state/state") >= 4);
        REQUIRE(server.getPublishCount("test_state/state") <= 12);
    }

    SECTION("with a narrower range keeps the wider poll") {
        config.mYAML["mqtt"]["objects"][0]["state"]["count"] = 2;
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 42);
        server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, 43);

        const std::string req = R"({"network":"tcptest","slave":1,"register":"2"})";
        REQUIRE(scheduledPollReads(server, req) >= 4);
        REQUIRE(server.mMqtt->rpcValue(1) == "42");
        REQUIRE(server.getPublishCount("test_state/state") >= 4);
        REQUIRE(server.getPublishCount("test_state/state") <= 12);
    }

    SECTION("with a disjoint range keeps the poll") {
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 42);
        server.setModbusRegisterValue("tcptest", 1, 5, modmqttd::RegisterType::HOLDING, 99);

        const std::string req = R"({"network":"tcptest","slave":1,"register":"5"})";
        REQUIRE(scheduledPollReads(server, req) >= 4);
        REQUIRE(server.mMqtt->rpcValue(1) == "99");
        REQUIRE(server.getPublishCount("test_state/state") >= 4);
        REQUIRE(server.getPublishCount("test_state/state") <= 12);
    }

    SECTION("returns zero when the polled register value is zero") {
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 0);
        server.start();
        server.waitForInitialPoll("tcptest");
        server.waitForSubscription("mqtt_test/rpc/modbus_request");

        const std::string req = R"({"network":"tcptest","slave":1,"register":"2"})";
        server.mMqtt->injectRpcRequest(
            "mqtt_test/rpc/modbus_request",
            req.c_str(), static_cast<int>(req.size()),
            "test/response", 1);

        server.waitForRpcResponse(1);
        REQUIRE(server.mMqtt->rpcValue(1) == "0");
        REQUIRE(server.mMqtt->rpcUserProperty(1, "error").empty());
        server.stop();
    }
}


TEST_CASE("RPC read feeding an on_change poll") {

    // In on_change mode the state topic is published only when the value actually
    // changes - there is no rate limit. Routing RPC reads through object processing
    // must therefore (a) not flood the topic when the value is constant, and
    // (b) publish every change an RPC read observes, even while the scheduled poll
    // is deferred by those same RPC reads.
    TestConfig config(R"(
modbus:
  networks:
    - name: tcptest
      address: localhost
      port: 501
      slaves:
        - address: 1
mqtt:
  client_id: mqtt_test
  rpc:
    mode: read
  broker:
    host: localhost
  objects:
    - topic: test_state
      publish_mode: on_change
      state:
        register: tcptest.1.2
        refresh: 100ms
        register_type: holding
)");

    SECTION("does not flood the state topic on repeated reads of an unchanged value") {
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 42);
        server.start();
        server.waitForSubscription("mqtt_test/rpc/modbus_request");

        const std::string req = R"({"network":"tcptest","slave":1,"register":"2"})";
        for (int i = 1; i <= 30; i++) {
            server.mMqtt->injectRpcRequest(
                "mqtt_test/rpc/modbus_request",
                req.c_str(), static_cast<int>(req.size()),
                "test/response", i);
            std::this_thread::sleep_for(timing::milliseconds(20));
        }
        for (int i = 1; i <= 30; i++) {
            server.waitForRpcResponse(i);
        }
        server.stop();

        // 30 RPC reads of a constant value must not produce 30 state publishes
        REQUIRE(server.getPublishCount("test_state/state") <= 2);
    }

    SECTION("publishes every changed value an RPC read observes") {
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 0);
        server.start();
        server.waitForSubscription("mqtt_test/rpc/modbus_request");

        const std::string req = R"({"network":"tcptest","slave":1,"register":"2"})";
        for (int i = 1; i <= 5; i++) {
            server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, i * 10);
            server.mMqtt->injectRpcRequest(
                "mqtt_test/rpc/modbus_request",
                req.c_str(), static_cast<int>(req.size()),
                "test/response", i);
            server.waitForRpcResponse(i);
            // each distinct value read by RPC reaches the poll state topic
            server.waitForMqttValue("test_state/state", std::to_string(i * 10).c_str());
        }
        server.stop();
    }
}


TEST_CASE("RPC error handling") {

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
      slaves:
        - address: 1
mqtt:
  client_id: mqtt_test
  rpc:
    mode: read
  broker:
    host: localhost
  objects: []
)");

    SECTION("bad JSON should return error property") {
        MockedModMqttServerThread server(config.toString());
        server.start();
        server.waitForSubscription("mqtt_test/rpc/modbus_request");

        const std::string req = "not json";
        server.mMqtt->injectRpcRequest(
            "mqtt_test/rpc/modbus_request",
            req.c_str(), static_cast<int>(req.size()),
            "test/response", 1);

        server.waitForRpcResponse(1);
        REQUIRE(server.mMqtt->rpcValue(1).empty());
        REQUIRE(!server.mMqtt->rpcUserProperty(1, "error").empty());
        server.stop();
    }

    SECTION("network not found should return error property") {
        MockedModMqttServerThread server(config.toString());
        server.start();
        server.waitForSubscription("mqtt_test/rpc/modbus_request");

        const std::string req = R"({"network":"nope","slave":1,"register":"2"})";
        server.mMqtt->injectRpcRequest(
            "mqtt_test/rpc/modbus_request",
            req.c_str(), static_cast<int>(req.size()),
            "test/response", 1);

        server.waitForRpcResponse(1);
        REQUIRE(server.mMqtt->rpcValue(1).empty());
        REQUIRE(!server.mMqtt->rpcUserProperty(1, "error").empty());
        server.stop();
    }

    SECTION("unknown register_type should return error property") {
        MockedModMqttServerThread server(config.toString());
        server.start();
        server.waitForSubscription("mqtt_test/rpc/modbus_request");

        const std::string req = R"({"network":"tcptest","slave":1,"register":"2","register_type":"banana"})";
        server.mMqtt->injectRpcRequest(
            "mqtt_test/rpc/modbus_request",
            req.c_str(), static_cast<int>(req.size()),
            "test/response", 1);

        server.waitForRpcResponse(1);
        REQUIRE(server.mMqtt->rpcValue(1).empty());
        REQUIRE(!server.mMqtt->rpcUserProperty(1, "error").empty());
        server.stop();
    }

    SECTION("unknown converter should return error property") {
        MockedModMqttServerThread server(config.toString());
        server.start();
        server.waitForSubscription("mqtt_test/rpc/modbus_request");

        const std::string req = R"json({"network":"tcptest","slave":1,"register":"2","converter":"std.nope()"})json";
        server.mMqtt->injectRpcRequest(
            "mqtt_test/rpc/modbus_request",
            req.c_str(), static_cast<int>(req.size()),
            "test/response", 1);

        server.waitForRpcResponse(1);
        REQUIRE(server.mMqtt->rpcValue(1).empty());
        REQUIRE(!server.mMqtt->rpcUserProperty(1, "error").empty());
        server.stop();
    }

    SECTION("malformed converter spec should return error property") {
        MockedModMqttServerThread server(config.toString());
        server.start();
        server.waitForSubscription("mqtt_test/rpc/modbus_request");

        const std::string req = R"json({"network":"tcptest","slave":1,"register":"2","converter":"not a spec"})json";
        server.mMqtt->injectRpcRequest(
            "mqtt_test/rpc/modbus_request",
            req.c_str(), static_cast<int>(req.size()),
            "test/response", 1);

        server.waitForRpcResponse(1);
        REQUIRE(server.mMqtt->rpcValue(1).empty());
        REQUIRE(!server.mMqtt->rpcUserProperty(1, "error").empty());
        server.stop();
    }

    SECTION("non-string converter field should return error property") {
        MockedModMqttServerThread server(config.toString());
        server.start();
        server.waitForSubscription("mqtt_test/rpc/modbus_request");

        const std::string req = R"json({"network":"tcptest","slave":1,"register":"2","converter":42})json";
        server.mMqtt->injectRpcRequest(
            "mqtt_test/rpc/modbus_request",
            req.c_str(), static_cast<int>(req.size()),
            "test/response", 1);

        server.waitForRpcResponse(1);
        REQUIRE(server.mMqtt->rpcValue(1).empty());
        REQUIRE(!server.mMqtt->rpcUserProperty(1, "error").empty());
        server.stop();
    }
}


TEST_CASE("RPC write request") {

    TestConfig config(R"(
modbus:
  networks:
    - name: tcptest
      address: localhost
      port: 501
      slaves:
        - address: 1
          delay_before_command: 50ms
mqtt:
  client_id: mqtt_test
  rpc:
    mode: readwrite
  broker:
    host: localhost
  objects: []
)");

    SECTION("in readwrite mode should return scalar value written") {
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 0);
        server.start();
        server.waitForSubscription("mqtt_test/rpc/modbus_request");

        const std::string req = R"({"network":"tcptest","slave":1,"register":"2","value":77})";
        server.mMqtt->injectRpcRequest(
            "mqtt_test/rpc/modbus_request",
            req.c_str(), static_cast<int>(req.size()),
            "test/response", 1);

        server.waitForRpcResponse(1);
        REQUIRE(server.mMqtt->rpcValue(1) == "77");
        REQUIRE(server.mMqtt->rpcUserProperty(1, "error").empty());
        server.waitForModbusValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 77);
        server.stop();
    }

    SECTION("multiple registers should return JSON array of values written") {
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 0);
        server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, 0);
        server.start();
        server.waitForSubscription("mqtt_test/rpc/modbus_request");

        const std::string req = R"({"network":"tcptest","slave":1,"register":"2","value":[10,20]})";
        server.mMqtt->injectRpcRequest(
            "mqtt_test/rpc/modbus_request",
            req.c_str(), static_cast<int>(req.size()),
            "test/response", 1);

        server.waitForRpcResponse(1);
        REQUIRE(server.mMqtt->rpcValue(1) == "[10,20]");
        REQUIRE(server.mMqtt->rpcUserProperty(1, "error").empty());
        server.waitForModbusValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 10);
        server.waitForModbusValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, 20);
        server.stop();
    }

    SECTION("in read mode should return error property") {
        config.mYAML["mqtt"]["rpc"]["mode"] = "read";
        MockedModMqttServerThread server(config.toString());
        server.start();
        server.waitForSubscription("mqtt_test/rpc/modbus_request");

        const std::string req = R"({"network":"tcptest","slave":1,"register":"2","value":77})";
        server.mMqtt->injectRpcRequest(
            "mqtt_test/rpc/modbus_request",
            req.c_str(), static_cast<int>(req.size()),
            "test/response", 1);

        server.waitForRpcResponse(1);
        REQUIRE(server.mMqtt->rpcValue(1).empty());
        REQUIRE(!server.mMqtt->rpcUserProperty(1, "error").empty());
        server.stop();
    }

    SECTION("to read-only register type should return error property") {
        MockedModMqttServerThread server(config.toString());
        server.start();
        server.waitForSubscription("mqtt_test/rpc/modbus_request");

        const std::string req = R"({"network":"tcptest","slave":1,"register":"2","register_type":"input","value":77})";
        server.mMqtt->injectRpcRequest(
            "mqtt_test/rpc/modbus_request",
            req.c_str(), static_cast<int>(req.size()),
            "test/response", 1);

        server.waitForRpcResponse(1);
        REQUIRE(server.mMqtt->rpcValue(1).empty());
        REQUIRE(!server.mMqtt->rpcUserProperty(1, "error").empty());
        server.stop();
    }
}


TEST_CASE("RPC read failure") {

    TestConfig config(R"(
modbus:
  networks:
    - name: tcptest
      address: localhost
      port: 501
      read_retries: 0
      slaves:
        - address: 1
          delay_before_command: 50ms
mqtt:
  client_id: mqtt_test
  rpc:
    mode: read
  broker:
    host: localhost
  objects: []
)");

    SECTION("should return error property after retries") {
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterReadError("tcptest", 1, 3, modmqttd::RegisterType::HOLDING);
        server.start();
        server.waitForSubscription("mqtt_test/rpc/modbus_request");

        const std::string req = R"({"network":"tcptest","slave":1,"register":"3"})";
        server.mMqtt->injectRpcRequest(
            "mqtt_test/rpc/modbus_request",
            req.c_str(), static_cast<int>(req.size()),
            "test/response", 1);

        server.waitForRpcResponse(1);
        REQUIRE(server.mMqtt->rpcValue(1).empty());
        REQUIRE(!server.mMqtt->rpcUserProperty(1, "error").empty());
        server.stop();
    }
}


TEST_CASE("RPC multiple requests in flight") {

    TestConfig config(R"(
modbus:
  networks:
    - name: tcptest
      address: localhost
      port: 501
      slaves:
        - address: 1
          delay_before_command: 50ms
mqtt:
  client_id: mqtt_test
  rpc:
    mode: read
  broker:
    host: localhost
  objects: []
)");

    SECTION("should match each response to its correlation data") {
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 11);
        server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, 22);
        server.start();
        server.waitForSubscription("mqtt_test/rpc/modbus_request");

        const std::string req1 = R"({"network":"tcptest","slave":1,"register":"2"})";
        const std::string req2 = R"({"network":"tcptest","slave":1,"register":"3"})";
        server.mMqtt->injectRpcRequest(
            "mqtt_test/rpc/modbus_request",
            req1.c_str(), static_cast<int>(req1.size()),
            "test/response", 1);
        server.mMqtt->injectRpcRequest(
            "mqtt_test/rpc/modbus_request",
            req2.c_str(), static_cast<int>(req2.size()),
            "test/response", 2);

        server.waitForRpcResponse(1);
        server.waitForRpcResponse(2);
        REQUIRE(server.mMqtt->rpcValue(1) == "11");
        REQUIRE(server.mMqtt->rpcValue(2) == "22");
        server.stop();
    }
}


TEST_CASE("RPC read with converter") {

    // TestNumbers::Float::{AB,CD} are the two registers of -1.234567 in ABCD order
    // (the same fixture stdconv_float_tests uses for std.float32).

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
      slaves:
        - address: 1
          delay_before_command: 50ms
mqtt:
  client_id: mqtt_test
  rpc:
    mode: read
  broker:
    host: localhost
  objects: []
)");

    SECTION("encodes the reply as the converter's scalar value") {
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, TestNumbers::Float::AB);
        server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, TestNumbers::Float::CD);
        server.start();
        server.waitForSubscription("mqtt_test/rpc/modbus_request");

        const std::string req = R"json({"network":"tcptest","slave":1,"register":"2","count":2,"converter":"std.float32()"})json";
        server.mMqtt->injectRpcRequest(
            "mqtt_test/rpc/modbus_request",
            req.c_str(), static_cast<int>(req.size()),
            "test/response", 1);

        server.waitForRpcResponse(1);
        REQUIRE(server.mMqtt->rpcValue(1) == "-1.234567");
        REQUIRE(server.mMqtt->rpcUserProperty(1, "error").empty());
        server.stop();
    }

    SECTION("applies converter arguments") {
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, TestNumbers::Float::AB);
        server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, TestNumbers::Float::CD);
        server.start();
        server.waitForSubscription("mqtt_test/rpc/modbus_request");

        const std::string req = R"json({"network":"tcptest","slave":1,"register":"2","count":2,"converter":"std.float32(precision=2)"})json";
        server.mMqtt->injectRpcRequest(
            "mqtt_test/rpc/modbus_request",
            req.c_str(), static_cast<int>(req.size()),
            "test/response", 1);

        server.waitForRpcResponse(1);
        REQUIRE(server.mMqtt->rpcValue(1) == "-1.23");
        REQUIRE(server.mMqtt->rpcUserProperty(1, "error").empty());
        server.stop();
    }

    SECTION("converter \"none\" falls back to the raw value array") {
        const std::string expected =
            "[" + std::to_string(TestNumbers::Float::AB) + "," + std::to_string(TestNumbers::Float::CD) + "]";

        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, TestNumbers::Float::AB);
        server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, TestNumbers::Float::CD);
        server.start();
        server.waitForSubscription("mqtt_test/rpc/modbus_request");

        const std::string req = R"json({"network":"tcptest","slave":1,"register":"2","count":2,"converter":"none"})json";
        server.mMqtt->injectRpcRequest(
            "mqtt_test/rpc/modbus_request",
            req.c_str(), static_cast<int>(req.size()),
            "test/response", 1);

        server.waitForRpcResponse(1);
        REQUIRE(server.mMqtt->rpcValue(1) == expected);
        REQUIRE(server.mMqtt->rpcUserProperty(1, "error").empty());
        server.stop();
    }

    SECTION("empty converter falls back to the raw value array") {
        const std::string expected =
            "[" + std::to_string(TestNumbers::Float::AB) + "," + std::to_string(TestNumbers::Float::CD) + "]";

        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, TestNumbers::Float::AB);
        server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, TestNumbers::Float::CD);
        server.start();
        server.waitForSubscription("mqtt_test/rpc/modbus_request");

        const std::string req = R"json({"network":"tcptest","slave":1,"register":"2","count":2,"converter":""})json";
        server.mMqtt->injectRpcRequest(
            "mqtt_test/rpc/modbus_request",
            req.c_str(), static_cast<int>(req.size()),
            "test/response", 1);

        server.waitForRpcResponse(1);
        REQUIRE(server.mMqtt->rpcValue(1) == expected);
        REQUIRE(server.mMqtt->rpcUserProperty(1, "error").empty());
        server.stop();
    }

    SECTION("converter that needs two registers fails on a single-register read") {
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, TestNumbers::Float::AB);
        server.start();
        server.waitForSubscription("mqtt_test/rpc/modbus_request");

        const std::string req = R"json({"network":"tcptest","slave":1,"register":"2","converter":"std.float32()"})json";
        server.mMqtt->injectRpcRequest(
            "mqtt_test/rpc/modbus_request",
            req.c_str(), static_cast<int>(req.size()),
            "test/response", 1);

        server.waitForRpcResponse(1);
        REQUIRE(server.mMqtt->rpcValue(1).empty());
        REQUIRE(!server.mMqtt->rpcUserProperty(1, "error").empty());
        server.stop();
    }
}


TEST_CASE("RPC write with converter") {

    // -1.234567 encodes to TestNumbers::Float::{AB,CD} in std.float32's default ABCD order.

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
      slaves:
        - address: 1
          delay_before_command: 50ms
mqtt:
  client_id: mqtt_test
  rpc:
    mode: readwrite
  broker:
    host: localhost
  objects: []
)");

    SECTION("decodes the value into the converter's registers and echoes them raw") {
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 0);
        server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, 0);
        server.start();
        server.waitForSubscription("mqtt_test/rpc/modbus_request");

        const std::string req = R"json({"network":"tcptest","slave":1,"register":"2","count":2,"converter":"std.float32()","value":"-1.234567"})json";
        server.mMqtt->injectRpcRequest(
            "mqtt_test/rpc/modbus_request",
            req.c_str(), static_cast<int>(req.size()),
            "test/response", 1);

        server.waitForRpcResponse(1);
        // write reply is always raw register values (no converter re-applied on reply path)
        const std::string expected =
            "[" + std::to_string(TestNumbers::Float::AB) + "," + std::to_string(TestNumbers::Float::CD) + "]";
        REQUIRE(server.mMqtt->rpcValue(1) == expected);
        REQUIRE(server.mMqtt->rpcUserProperty(1, "error").empty());
        server.waitForModbusValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, TestNumbers::Float::AB);
        server.waitForModbusValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, TestNumbers::Float::CD);
        server.stop();
    }

    SECTION("round-trips a converter write back through a converter read") {
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 0);
        server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, 0);
        server.start();
        server.waitForSubscription("mqtt_test/rpc/modbus_request");

        const std::string writeReq = R"json({"network":"tcptest","slave":1,"register":"2","count":2,"converter":"std.float32()","value":"-1.234567"})json";
        server.mMqtt->injectRpcRequest(
            "mqtt_test/rpc/modbus_request",
            writeReq.c_str(), static_cast<int>(writeReq.size()),
            "test/response", 1);
        server.waitForRpcResponse(1);

        const std::string readReq = R"json({"network":"tcptest","slave":1,"register":"2","count":2,"converter":"std.float32()"})json";
        server.mMqtt->injectRpcRequest(
            "mqtt_test/rpc/modbus_request",
            readReq.c_str(), static_cast<int>(readReq.size()),
            "test/response", 2);
        server.waitForRpcResponse(2);

        REQUIRE(server.mMqtt->rpcValue(2) == "-1.234567");
        REQUIRE(server.mMqtt->rpcUserProperty(2, "error").empty());
        server.stop();
    }

    SECTION("array value with a converter should return error property") {
        MockedModMqttServerThread server(config.toString());
        server.start();
        server.waitForSubscription("mqtt_test/rpc/modbus_request");

        const std::string req = R"json({"network":"tcptest","slave":1,"register":"2","count":2,"converter":"std.float32()","value":[1,2]})json";
        server.mMqtt->injectRpcRequest(
            "mqtt_test/rpc/modbus_request",
            req.c_str(), static_cast<int>(req.size()),
            "test/response", 1);

        server.waitForRpcResponse(1);
        REQUIRE(server.mMqtt->rpcValue(1).empty());
        REQUIRE(!server.mMqtt->rpcUserProperty(1, "error").empty());
        server.stop();
    }

    SECTION("converter that needs two registers fails on a single-register write") {
        MockedModMqttServerThread server(config.toString());
        server.start();
        server.waitForSubscription("mqtt_test/rpc/modbus_request");

        const std::string req = R"json({"network":"tcptest","slave":1,"register":"2","count":1,"converter":"std.float32()","value":"-1.234567"})json";
        server.mMqtt->injectRpcRequest(
            "mqtt_test/rpc/modbus_request",
            req.c_str(), static_cast<int>(req.size()),
            "test/response", 1);

        server.waitForRpcResponse(1);
        REQUIRE(server.mMqtt->rpcValue(1).empty());
        REQUIRE(!server.mMqtt->rpcUserProperty(1, "error").empty());
        server.stop();
    }
}
