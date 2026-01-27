#include <catch2/catch_all.hpp>
#include "libmodmqttconv/converterplugin.hpp"
#include "libmodmqttsrv/config.hpp"
#include "libmodmqttsrv/dll_import.hpp"

#include "testnumbers.hpp"
#include "plugin_utils.hpp"

#ifdef HAVE_EXPRTK

TEST_CASE ("exprtk should read") {
    PluginLoader loader("../exprconv/exprconv.so");

    std::shared_ptr<DataConverter> conv(loader.getConverter("evaluate"));
    ConverterArgValues args(conv->getArgs());

    SECTION("int32 from two registers") {
        args.setArgValue("expression", "int32(R0, R1)");
        const ModbusRegisters input({TestNumbers::Int::AB, TestNumbers::Int::CD});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt() == TestNumbers::Int::ABCD_as_int32);
    }

    SECTION("int32 from two registers byte swapped") {
        args.setArgValue("expression", "int32bs(R0, R1)");
        const ModbusRegisters input({TestNumbers::Int::BA, TestNumbers::Int::DC});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt() == TestNumbers::Int::ABCD_as_int32);
    }


    SECTION("uint32 from two registers") {
        args.setArgValue("expression", "uint32(R0, R1)");
        const ModbusRegisters input({TestNumbers::Int::AB, TestNumbers::Int::CD});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt64() == TestNumbers::Int::ABCD_as_uint32);
    }

    SECTION("uint32 from two registers byte swapped") {
        args.setArgValue("expression", "uint32bs(R0, R1)");
        const ModbusRegisters input({TestNumbers::Int::BA, TestNumbers::Int::DC});

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getInt64() == TestNumbers::Int::ABCD_as_uint32);
    }
}

#endif
