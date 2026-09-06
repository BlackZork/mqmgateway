#include <libmodmqttsrv/config.hpp>
#include <catch2/catch_all.hpp>
#include "libmodmqttsrv/dll_import.hpp"

#include "libmodmqttconv/converterplugin.hpp"
#include "libmodmqttconv/convexception.hpp"

#include "mockedserver.hpp"
#include "testnumbers.hpp"
#include "plugin_utils.hpp"
#include "yaml_utils.hpp"

TEST_CASE("when std.uint64") {
    PluginLoader loader("../stdconv/stdconv.so");

    std::shared_ptr<DataConverter> conv(loader.getConverter("uint64"));
    ConverterArgValues args(conv->getArgs());

    SECTION("converts four modbus registers (high to low)") {
        ModbusRegisters input({TestNumbers::Int64::AB, TestNumbers::Int64::CD, TestNumbers::Int64::EF, TestNumbers::Int64::GH});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getUInt64() == TestNumbers::Int64::ABCDEFGH_as_uint64);
    }

    SECTION("converts four modbus registers (high to low) with byte swap") {
        args.setArgValue(ConverterArg::sSwapBytesArgName, "true");

        ModbusRegisters input({TestNumbers::Int64::BA, TestNumbers::Int64::DC, TestNumbers::Int64::FE, TestNumbers::Int64::HG});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getUInt64() == TestNumbers::Int64::ABCDEFGH_as_uint64);
    }

    SECTION("converts four modbus registers (low to high)") {
        args.setArgValue(ConverterArg::sLowFirstArgName, "true");

        ModbusRegisters input({TestNumbers::Int64::AB, TestNumbers::Int64::CD, TestNumbers::Int64::EF, TestNumbers::Int64::GH});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getUInt64() == TestNumbers::Int64::GHEFCDAB_as_uint64);
    }

    SECTION("converts four modbus registers (low to high) with byte swap") {
        args.setArgValue(ConverterArg::sLowFirstArgName, "true");
        args.setArgValue(ConverterArg::sSwapBytesArgName, "true");

        ModbusRegisters input({TestNumbers::Int64::BA, TestNumbers::Int64::DC, TestNumbers::Int64::FE, TestNumbers::Int64::HG});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getUInt64() == TestNumbers::Int64::GHEFCDAB_as_uint64);
    }

    SECTION("publishes the largest uint64 without wrapping") {
        ModbusRegisters input({0xffff, 0xffff, 0xffff, 0xffff});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getString() == "18446744073709551615");
    }

    SECTION("converts fewer registers without sign extension") {
        ModbusRegisters input({TestNumbers::Int::AB, TestNumbers::Int::CD});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getUInt64() == TestNumbers::Int::ABCD_as_uint32);
    }

    SECTION("writes to four modbus registers (high to low)") {
        MqttValue input = MqttValue::fromUInt64(TestNumbers::Int64::ABCDEFGH_as_uint64);

        conv->setArgValues(args);
        ModbusRegisters output = conv->toModbus(input, 4);

        REQUIRE(output.values() == std::vector<uint16_t>({TestNumbers::Int64::AB, TestNumbers::Int64::CD, TestNumbers::Int64::EF, TestNumbers::Int64::GH}));
    }

    SECTION("writes to four modbus registers (high to low) with byte swap") {
        args.setArgValue(ConverterArg::sSwapBytesArgName, "true");

        MqttValue input = MqttValue::fromUInt64(TestNumbers::Int64::ABCDEFGH_as_uint64);

        conv->setArgValues(args);
        ModbusRegisters output = conv->toModbus(input, 4);

        REQUIRE(output.values() == std::vector<uint16_t>({TestNumbers::Int64::BA, TestNumbers::Int64::DC, TestNumbers::Int64::FE, TestNumbers::Int64::HG}));
    }

    SECTION("writes to four modbus registers (low to high)") {
        args.setArgValue(ConverterArg::sLowFirstArgName, "true");

        MqttValue input = MqttValue::fromUInt64(TestNumbers::Int64::ABCDEFGH_as_uint64);

        conv->setArgValues(args);
        ModbusRegisters output = conv->toModbus(input, 4);

        REQUIRE(output.values() == std::vector<uint16_t>({TestNumbers::Int64::GH, TestNumbers::Int64::EF, TestNumbers::Int64::CD, TestNumbers::Int64::AB}));
    }

    SECTION("writes to four modbus registers (low to high) with byte swap") {
        args.setArgValue(ConverterArg::sLowFirstArgName, "true");
        args.setArgValue(ConverterArg::sSwapBytesArgName, "true");

        MqttValue input = MqttValue::fromUInt64(TestNumbers::Int64::ABCDEFGH_as_uint64);

        conv->setArgValues(args);
        ModbusRegisters output = conv->toModbus(input, 4);

        REQUIRE(output.values() == std::vector<uint16_t>({TestNumbers::Int64::HG, TestNumbers::Int64::FE, TestNumbers::Int64::DC, TestNumbers::Int64::BA}));
    }

    // a configured command always hands the converter its payload as a string,
    // and getUInt64() parses those with strtoull, which wraps a leading minus
    SECTION("writes an mqtt payload above INT64_MAX") {
        std::string val("11651590505119487784");
        MqttValue input = MqttValue::fromBinary(val.c_str(), val.length());

        conv->setArgValues(args);
        ModbusRegisters output = conv->toModbus(input, 4);

        REQUIRE(output.values() == std::vector<uint16_t>({TestNumbers::Int64::AB, TestNumbers::Int64::CD, TestNumbers::Int64::EF, TestNumbers::Int64::GH}));
    }

    SECTION("rejects a negative mqtt payload") {
        std::string val("-1");
        MqttValue input = MqttValue::fromBinary(val.c_str(), val.length());

        conv->setArgValues(args);

        REQUIRE_THROWS_AS(conv->toModbus(input, 4), ConvException);
    }
}

TEST_CASE("std.uint64 on object state topic") {
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
    - topic: test_sensor
      state:
        register: tcptest.1.2
        register_type: input
        count: 4
        converter: std.uint64()
)");

    SECTION("should publish a value above INT64_MAX as a positive number") {
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, TestNumbers::Int64::AB);
        server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::INPUT, TestNumbers::Int64::CD);
        server.setModbusRegisterValue("tcptest", 1, 4, modmqttd::RegisterType::INPUT, TestNumbers::Int64::EF);
        server.setModbusRegisterValue("tcptest", 1, 5, modmqttd::RegisterType::INPUT, TestNumbers::Int64::GH);
        server.start();
        server.waitForPublish("test_sensor/state");

        REQUIRE(server.mqttValue("test_sensor/state") == "11651590505119487784");

        server.stop();
    }
}
