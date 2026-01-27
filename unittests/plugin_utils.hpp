#pragma once

#include <libmodmqttsrv/dll_import.hpp>
#include <libmodmqttconv/converterplugin.hpp>

/**
 * Ensure correct destruction for
 * dll library and converter instance
 */
class PluginLoader {
    public:
        PluginLoader(const std::string& convPath) {
            mPlugin = modmqttd::dll_import<ConverterPlugin>(
                convPath,
                "converter_plugin"
            );
        }

        const std::shared_ptr<DataConverter>& getConverter(const std::string& name) {
            mConverter.reset(mPlugin->getConverter(name));
            return mConverter;
        }

        ~PluginLoader() {
            mConverter.reset();
            mPlugin.reset();
        }
    private:
        std::shared_ptr<ConverterPlugin> mPlugin;
        std::shared_ptr<DataConverter> mConverter;
};
