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
    ConverterArgValues args(conv->getArgs());

    SECTION("int32 from two registers") {
        args.setArgValue("expression", ConverterArgType::STRING, "int32(R0, R1)");
        const ModbusRegisters input({TestNumbers::Int::AB, TestNumbers::Int::CD});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt() == TestNumbers::Int::ABCD_as_int32);
    }

    SECTION("int32 from two registers byte swapped") {
        args.setArgValue("expression", ConverterArgType::STRING, "int32bs(R0, R1)");
        const ModbusRegisters input({TestNumbers::Int::BA, TestNumbers::Int::DC});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt() == TestNumbers::Int::ABCD_as_int32);
    }


    SECTION("uint32 from two registers") {
        args.setArgValue("expression", ConverterArgType::STRING, "uint32(R0, R1)");
        const ModbusRegisters input({TestNumbers::Int::AB, TestNumbers::Int::CD});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt64() == TestNumbers::Int::ABCD_as_uint32);
    }

    SECTION("uint32 from two registers byte swapped") {
        args.setArgValue("expression", ConverterArgType::STRING, "uint32bs(R0, R1)");
        const ModbusRegisters input({TestNumbers::Int::BA, TestNumbers::Int::DC});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt64() == TestNumbers::Int::ABCD_as_uint32);
    }
}

#endif
