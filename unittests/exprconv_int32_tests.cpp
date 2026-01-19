#include <catch2/catch_all.hpp>
#include "libmodmqttconv/converterplugin.hpp"
#include "libmodmqttsrv/config.hpp"
#include "libmodmqttsrv/dll_import.hpp"

#include "testnumbers.hpp"

#ifdef HAVE_EXPRTK

TEST_CASE ("exprtk should read") {
    std::string stdconv_path = "../exprconv/exprconv.so";
    std::shared_ptr<ConverterPlugin> plugin = modmqttd::dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin"
    );
    std::shared_ptr<DataConverter> conv(plugin->getConverter("evaluate"));

    SECTION("int32 from two registers") {
        conv->setArgs({"int32(R0, R1)"});
        const ModbusRegisters input({TestNumbers::Int::AB, TestNumbers::Int::CD});

        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt() == TestNumbers::Int::ABCD_as_int32);
    }

    SECTION("int32 from two registers byte swapped") {
        conv->setArgs({"int32bs(R0, R1)"});
        const ModbusRegisters input({TestNumbers::Int::BA, TestNumbers::Int::DC});

        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt() == TestNumbers::Int::ABCD_as_int32);
    }


    SECTION("uint32 from two registers") {
        conv->setArgs({"uint32(R0, R1)"});
        const ModbusRegisters input({TestNumbers::Int::AB, TestNumbers::Int::CD});

        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt64() == TestNumbers::Int::ABCD_as_uint32);
    }

    SECTION("uint32 from two registers byte swapped") {
        conv->setArgs({"uint32bs(R0, R1)"});
        const ModbusRegisters input({TestNumbers::Int::BA, TestNumbers::Int::DC});

        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt64() == TestNumbers::Int::ABCD_as_uint32);
    }
}

#endif
