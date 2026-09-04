#pragma once

#include "converter.hpp"
#include <string>

/**
 * Version of the interface a converter plugin shares with modmqttd. Bump it
 * whenever anything crossing that boundary changes shape or meaning: MqttValue,
 * ModbusRegisters, DataConverter, ConverterArgs.
 *
 * These types are header-only, so a plugin carries its own inlined copy of
 * them. One built against a different version disagrees about their layout, and
 * calling into it would corrupt memory rather than fail cleanly. Every plugin
 * must therefore export the marker
 *
 *     extern "C" const int converter_plugin_abi_version = CONVERTER_ABI_VERSION;
 *
 * next to its converter_plugin symbol. modmqttd reads it before touching the
 * plugin object and refuses to load a plugin that reports anything else.
 */
constexpr int CONVERTER_ABI_VERSION = 1;

class ConverterPlugin {
    public:
        virtual std::string getName() const = 0;

        virtual DataConverter* getConverter(const std::string& name) = 0;

        virtual ~ConverterPlugin() {
        };
};
