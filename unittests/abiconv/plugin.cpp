#include "libmodmqttconv/converterplugin.hpp"

/**
 * Converter plugins that modmqttd must refuse, so that the checks in
 * ModMqtt::initConverterPlugin are exercised by a real dlopen rather than
 * assumed. They convert nothing, loading never gets that far.
 *
 * Built twice: badabiconv.so reports the wrong ABI version, noabiconv.so omits
 * the marker entirely, the way a plugin predating ABI versioning does.
 */
#ifndef OMIT_ABI_VERSION
extern "C" const int converter_plugin_abi_version = CONVERTER_ABI_VERSION + 1;
#endif

class BadAbiConvPlugin : public ConverterPlugin {
    public:
        virtual std::string getName() const { return "badabi"; }
        virtual DataConverter* getConverter(const std::string& pName) { return nullptr; }
        virtual ~BadAbiConvPlugin() {}
};

extern "C" BadAbiConvPlugin converter_plugin;
BadAbiConvPlugin converter_plugin;
