#include <catch2/catch_all.hpp>

#include "libmodmqttconv/converterplugin.hpp"
#include "libmodmqttconv/convexception.hpp"
#include "libmodmqttconv/convtools.hpp"
#include "libmodmqttsrv/config.hpp"

#include <rapidjson/document.h>

#include "mockedserver.hpp"
#include "plugin_utils.hpp"
#include "testnumbers.hpp"
#include "yaml_utils.hpp"

static rapidjson::Document parseOutput(DataConverter& pConv, const ModbusRegisters& pData) {
    MqttValue out = pConv.toMqtt(pData);
    std::string json = out.getString();
    rapidjson::Document doc;
    // rapidjson's default float parsing is only accurate to about one ULP, which
    // is not enough to check that a 64-bit double survived the round trip
    REQUIRE_FALSE(doc.Parse<rapidjson::kParseFullPrecisionFlag>(json.c_str()).HasParseError());
    return doc;
}

TEST_CASE("std.debug for single register") {
    PluginLoader loader("../stdconv/stdconv.so");
    std::shared_ptr<DataConverter> conv(loader.getConverter("debug"));
    REQUIRE(conv != nullptr);

    const ModbusRegisters input({TestNumbers::Int::AB});

    SECTION("should output raw and hex arrays") {
        auto doc = parseOutput(*conv, input);
        REQUIRE(doc["raw"][0].GetUint() == TestNumbers::Int::AB);
        REQUIRE(std::string(doc["hex"][0].GetString()) == "0xA1B2");
    }

    SECTION("should output int16 variants with converter call strings as keys") {
        auto doc = parseOutput(*conv, input);
        REQUIRE(doc["int16"]["std.int16"].GetInt() == TestNumbers::Int::AB_as_int16);
        int16_t swapped = ConverterTools::setByteOrder(TestNumbers::Int::AB, true);
        REQUIRE(doc["int16"]["std.int16(swap_bytes=true)"].GetInt() == swapped);
    }

    SECTION("should output uint16 variants with converter call strings as keys") {
        auto doc = parseOutput(*conv, input);
        REQUIRE(doc["uint16"]["std.uint16"].GetUint() == TestNumbers::Int::AB_as_uint16);
        uint16_t swapped = ConverterTools::setByteOrder(TestNumbers::Int::AB, true);
        REQUIRE(doc["uint16"]["std.uint16(swap_bytes=true)"].GetUint() == swapped);
    }

    SECTION("should not output int32/uint32/float32 sections") {
        auto doc = parseOutput(*conv, input);
        REQUIRE_FALSE(doc.HasMember("int32"));
        REQUIRE_FALSE(doc.HasMember("uint32"));
        REQUIRE_FALSE(doc.HasMember("float32"));
    }

    SECTION("should output a string section") {
        auto doc = parseOutput(*conv, input);
        REQUIRE(doc.HasMember("string"));
    }

    SECTION("toModbus should throw ConvException") {
        MqttValue dummy = MqttValue::fromInt(0);
        REQUIRE_THROWS_AS(conv->toModbus(dummy, 1), ConvException);
    }
}

TEST_CASE("std.debug for two registers") {
    PluginLoader loader("../stdconv/stdconv.so");
    std::shared_ptr<DataConverter> conv(loader.getConverter("debug"));

    const ModbusRegisters input({TestNumbers::Int::AB, TestNumbers::Int::CD});

    SECTION("should output all four int32 variants") {
        auto doc = parseOutput(*conv, input);
        REQUIRE(doc["int32"]["std.int32"].GetInt() == TestNumbers::Int::ABCD_as_int32);
        REQUIRE(doc["int32"]["std.int32(low_first=true)"].GetInt() == TestNumbers::Int::CDAB_as_int32);
        REQUIRE(doc["int32"]["std.int32(swap_bytes=true)"].GetInt() == TestNumbers::Int::BADC_as_int32);
        REQUIRE(doc["int32"]["std.int32(low_first=true,swap_bytes=true)"].GetInt() == TestNumbers::Int::DCBA_as_int32);
    }

    SECTION("should output all four uint32 variants") {
        auto doc = parseOutput(*conv, input);
        REQUIRE(doc["uint32"]["std.uint32"].GetUint() == TestNumbers::Int::ABCD_as_uint32);
        REQUIRE(doc["uint32"]["std.uint32(low_first=true)"].GetUint() == TestNumbers::Int::CDAB_as_uint32);
        REQUIRE(doc["uint32"]["std.uint32(swap_bytes=true)"].GetUint() == TestNumbers::Int::BADC_as_uint32);
        REQUIRE(doc["uint32"]["std.uint32(low_first=true,swap_bytes=true)"].GetUint() == TestNumbers::Int::DCBA_as_uint32);
    }

    SECTION("should not output int16/uint16 sections") {
        auto doc = parseOutput(*conv, input);
        REQUIRE_FALSE(doc.HasMember("int16"));
        REQUIRE_FALSE(doc.HasMember("uint16"));
    }

    SECTION("should output all four float32 variants") {
        auto doc = parseOutput(*conv, input);
        REQUIRE(doc["float32"].HasMember("std.float32"));
        REQUIRE(doc["float32"].HasMember("std.float32(low_first=true)"));
        REQUIRE(doc["float32"].HasMember("std.float32(swap_bytes=true)"));
        REQUIRE(doc["float32"].HasMember("std.float32(low_first=true,swap_bytes=true)"));
    }
}

TEST_CASE("std.debug for a register count no type fits") {
    PluginLoader loader("../stdconv/stdconv.so");
    std::shared_ptr<DataConverter> conv(loader.getConverter("debug"));

    const ModbusRegisters input({TestNumbers::Int::AB, TestNumbers::Int::CD, TestNumbers::Int::AB});

    SECTION("should output raw, hex and string sections") {
        auto doc = parseOutput(*conv, input);
        REQUIRE(doc.HasMember("raw"));
        REQUIRE(doc.HasMember("hex"));
        REQUIRE(doc.HasMember("string"));
        REQUIRE(doc["raw"].Size() == 3);
    }

    SECTION("should not output int32, uint32 or float32 sections") {
        auto doc = parseOutput(*conv, input);
        REQUIRE_FALSE(doc.HasMember("int32"));
        REQUIRE_FALSE(doc.HasMember("uint32"));
        REQUIRE_FALSE(doc.HasMember("float32"));
    }

    SECTION("should not output int16 or uint16 sections") {
        auto doc = parseOutput(*conv, input);
        REQUIRE_FALSE(doc.HasMember("int16"));
        REQUIRE_FALSE(doc.HasMember("uint16"));
    }

    SECTION("should not output 64-bit sections for five registers") {
        const ModbusRegisters fiveRegisters({TestNumbers::Int64::AB, TestNumbers::Int64::CD, TestNumbers::Int64::EF, TestNumbers::Int64::GH, TestNumbers::Int64::AB});

        auto doc = parseOutput(*conv, fiveRegisters);
        REQUIRE(doc["raw"].Size() == 5);
        REQUIRE_FALSE(doc.HasMember("int64"));
        REQUIRE_FALSE(doc.HasMember("uint64"));
        REQUIRE_FALSE(doc.HasMember("float64"));
    }
}

TEST_CASE("std.debug for four registers") {
    PluginLoader loader("../stdconv/stdconv.so");
    std::shared_ptr<DataConverter> conv(loader.getConverter("debug"));

    const ModbusRegisters input({TestNumbers::Int64::AB, TestNumbers::Int64::CD, TestNumbers::Int64::EF, TestNumbers::Int64::GH});

    SECTION("should output all four int64 variants") {
        auto doc = parseOutput(*conv, input);
        REQUIRE(doc["int64"]["std.int64"].GetInt64() == TestNumbers::Int64::ABCDEFGH_as_int64);
        REQUIRE(doc["int64"]["std.int64(low_first=true)"].GetInt64() == TestNumbers::Int64::GHEFCDAB_as_int64);
        REQUIRE(doc["int64"]["std.int64(swap_bytes=true)"].GetInt64() == TestNumbers::Int64::BADCFEHG_as_int64);
        REQUIRE(doc["int64"]["std.int64(low_first=true,swap_bytes=true)"].GetInt64() == TestNumbers::Int64::HGFEDCBA_as_int64);
    }

    SECTION("should output all four uint64 variants") {
        auto doc = parseOutput(*conv, input);
        REQUIRE(doc["uint64"]["std.uint64"].GetUint64() == TestNumbers::Int64::ABCDEFGH_as_uint64);
        REQUIRE(doc["uint64"]["std.uint64(low_first=true)"].GetUint64() == TestNumbers::Int64::GHEFCDAB_as_uint64);
        REQUIRE(doc["uint64"]["std.uint64(swap_bytes=true)"].GetUint64() == TestNumbers::Int64::BADCFEHG_as_uint64);
        REQUIRE(doc["uint64"]["std.uint64(low_first=true,swap_bytes=true)"].GetUint64() == TestNumbers::Int64::HGFEDCBA_as_uint64);
    }

    SECTION("should output the raw, hex and string sections") {
        auto doc = parseOutput(*conv, input);
        REQUIRE(doc["raw"].Size() == 4);
        REQUIRE(doc["hex"][0].GetString() == std::string("0xA1B2"));
        REQUIRE(doc.HasMember("string"));
    }

    SECTION("should not output 16 or 32-bit sections") {
        auto doc = parseOutput(*conv, input);
        REQUIRE_FALSE(doc.HasMember("int16"));
        REQUIRE_FALSE(doc.HasMember("int32"));
        REQUIRE_FALSE(doc.HasMember("uint32"));
        REQUIRE_FALSE(doc.HasMember("float32"));
    }
}

TEST_CASE("std.debug for float64") {
    PluginLoader loader("../stdconv/stdconv.so");
    std::shared_ptr<DataConverter> conv(loader.getConverter("debug"));

    SECTION("should output all four float64 variants") {
        const ModbusRegisters input({TestNumbers::Double::AB, TestNumbers::Double::CD, TestNumbers::Double::EF, TestNumbers::Double::GH});

        auto doc = parseOutput(*conv, input);
        REQUIRE_THAT(doc["float64"]["std.float64"].GetDouble(), Catch::Matchers::WithinULP(TestNumbers::Double::ABCDEFGH_as_double, 0));
        REQUIRE_THAT(doc["float64"]["std.float64(low_first=true)"].GetDouble(), Catch::Matchers::WithinULP(TestNumbers::Double::GHEFCDAB_as_double, 0));
        REQUIRE_THAT(doc["float64"]["std.float64(swap_bytes=true)"].GetDouble(), Catch::Matchers::WithinULP(TestNumbers::Double::BADCFEHG_as_double, 0));
        REQUIRE_THAT(doc["float64"]["std.float64(low_first=true,swap_bytes=true)"].GetDouble(), Catch::Matchers::WithinULP(TestNumbers::Double::HGFEDCBA_as_double, 0));
    }

    SECTION("should report nan and infinity as strings") {
        const ModbusRegisters nanInput(std::vector<uint16_t>(std::begin(TestNumbers::Double::NAN_REGISTERS), std::end(TestNumbers::Double::NAN_REGISTERS)));
        auto nanDoc = parseOutput(*conv, nanInput);
        REQUIRE(nanDoc["float64"]["std.float64"].GetString() == std::string("nan"));

        const ModbusRegisters posInfInput(std::vector<uint16_t>(std::begin(TestNumbers::Double::POS_INF_REGISTERS), std::end(TestNumbers::Double::POS_INF_REGISTERS)));
        auto posInfDoc = parseOutput(*conv, posInfInput);
        REQUIRE(posInfDoc["float64"]["std.float64"].GetString() == std::string("inf"));

        const ModbusRegisters negInfInput(std::vector<uint16_t>(std::begin(TestNumbers::Double::NEG_INF_REGISTERS), std::end(TestNumbers::Double::NEG_INF_REGISTERS)));
        auto negInfDoc = parseOutput(*conv, negInfInput);
        REQUIRE(negInfDoc["float64"]["std.float64"].GetString() == std::string("-inf"));
    }
}

TEST_CASE("std.debug for float32") {
    PluginLoader loader("../stdconv/stdconv.so");
    std::shared_ptr<DataConverter> conv(loader.getConverter("debug"));

    SECTION("should decode ABCD format as -1.234567") {
        const ModbusRegisters input({TestNumbers::Float::AB, TestNumbers::Float::CD});
        auto doc = parseOutput(*conv, input);
        REQUIRE_THAT((float)doc["float32"]["std.float32"].GetDouble(),
                     Catch::Matchers::WithinULP(TestNumbers::Float::ABCD_as_float, 0));
    }

    SECTION("should decode CDAB format via low_first=true") {
        const ModbusRegisters input({TestNumbers::Float::CD, TestNumbers::Float::AB});
        auto doc = parseOutput(*conv, input);
        REQUIRE_THAT((float)doc["float32"]["std.float32(low_first=true)"].GetDouble(),
                     Catch::Matchers::WithinULP(TestNumbers::Float::ABCD_as_float, 0));
    }
}

TEST_CASE("std.debug for special float32") {
    PluginLoader loader("../stdconv/stdconv.so");
    std::shared_ptr<DataConverter> conv(loader.getConverter("debug"));

    SECTION("should encode NaN as string \"nan\"") {
        const ModbusRegisters input({TestNumbers::Float::NAN_HIGH, TestNumbers::Float::NAN_LOW});
        auto doc = parseOutput(*conv, input);
        REQUIRE(doc["float32"]["std.float32"].IsString());
        REQUIRE(std::string(doc["float32"]["std.float32"].GetString()) == "nan");
    }

    SECTION("should encode +Inf as string \"inf\"") {
        const ModbusRegisters input({TestNumbers::Float::POS_INF_HIGH, TestNumbers::Float::POS_INF_LOW});
        auto doc = parseOutput(*conv, input);
        REQUIRE(doc["float32"]["std.float32"].IsString());
        REQUIRE(std::string(doc["float32"]["std.float32"].GetString()) == "inf");
    }

    SECTION("should encode -Inf as string \"-inf\"") {
        const ModbusRegisters input({TestNumbers::Float::NEG_INF_HIGH, TestNumbers::Float::NEG_INF_LOW});
        auto doc = parseOutput(*conv, input);
        REQUIRE(doc["float32"]["std.float32"].IsString());
        REQUIRE(std::string(doc["float32"]["std.float32"].GetString()) == "-inf");
    }
}

TEST_CASE("std.debug on object state topic") {
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
        count: 2
)");

    SECTION("should publish compact single-line JSON by default") {
        config.mYAML["mqtt"]["objects"][0]["state"]["converter"] = "std.debug()";
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, TestNumbers::Int::AB);
        server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::INPUT, TestNumbers::Int::CD);
        server.start();
        server.waitForPublish("test_sensor/state");

        std::string payload = server.mqttValue("test_sensor/state");

        REQUIRE(payload.find('\n') == std::string::npos);
        rapidjson::Document doc;
        REQUIRE_FALSE(doc.Parse(payload.c_str()).HasParseError());
        REQUIRE(doc["raw"][0].GetUint() == TestNumbers::Int::AB);
        REQUIRE(doc["raw"][1].GetUint() == TestNumbers::Int::CD);
        REQUIRE(doc.HasMember("hex"));
        REQUIRE(doc.HasMember("int32"));
        REQUIRE(doc.HasMember("uint32"));
        REQUIRE(doc.HasMember("float32"));
        REQUIRE(doc["int32"]["std.int32"].GetInt() == TestNumbers::Int::ABCD_as_int32);
        REQUIRE(doc["uint32"]["std.uint32"].GetUint() == TestNumbers::Int::ABCD_as_uint32);

        server.stop();
    }

    SECTION("should publish indented multi-line JSON with pretty_print=true") {
        config.mYAML["mqtt"]["objects"][0]["state"]["converter"] = "std.debug(pretty_print=true)";
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, TestNumbers::Int::AB);
        server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::INPUT, TestNumbers::Int::CD);
        server.start();
        server.waitForPublish("test_sensor/state");

        std::string payload = server.mqttValue("test_sensor/state");

        REQUIRE(payload.find('\n') != std::string::npos);
        rapidjson::Document doc;
        REQUIRE_FALSE(doc.Parse(payload.c_str()).HasParseError());
        REQUIRE(doc["raw"][0].GetUint() == TestNumbers::Int::AB);
        REQUIRE(doc["raw"][1].GetUint() == TestNumbers::Int::CD);
        REQUIRE(doc.HasMember("hex"));
        REQUIRE(doc.HasMember("int32"));
        REQUIRE(doc.HasMember("uint32"));
        REQUIRE(doc.HasMember("float32"));
        REQUIRE(doc["int32"]["std.int32"].GetInt() == TestNumbers::Int::ABCD_as_int32);
        REQUIRE(doc["uint32"]["std.uint32"].GetUint() == TestNumbers::Int::ABCD_as_uint32);

        server.stop();
    }

    SECTION("should publish the 64-bit sections for four registers") {
        config.mYAML["mqtt"]["objects"][0]["state"]["count"] = 4;
        config.mYAML["mqtt"]["objects"][0]["state"]["converter"] = "std.debug()";
        MockedModMqttServerThread server(config.toString());
        server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::INPUT, TestNumbers::Int64::AB);
        server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::INPUT, TestNumbers::Int64::CD);
        server.setModbusRegisterValue("tcptest", 1, 4, modmqttd::RegisterType::INPUT, TestNumbers::Int64::EF);
        server.setModbusRegisterValue("tcptest", 1, 5, modmqttd::RegisterType::INPUT, TestNumbers::Int64::GH);
        server.start();
        server.waitForPublish("test_sensor/state");

        std::string payload = server.mqttValue("test_sensor/state");

        rapidjson::Document doc;
        REQUIRE_FALSE(doc.Parse(payload.c_str()).HasParseError());
        REQUIRE(doc["raw"].Size() == 4);
        REQUIRE(doc["int64"]["std.int64"].GetInt64() == TestNumbers::Int64::ABCDEFGH_as_int64);
        REQUIRE(doc["uint64"]["std.uint64"].GetUint64() == TestNumbers::Int64::ABCDEFGH_as_uint64);
        REQUIRE(doc.HasMember("float64"));
        REQUIRE_FALSE(doc.HasMember("int32"));

        server.stop();
    }
}
