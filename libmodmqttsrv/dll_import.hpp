#pragma once

#include <dlfcn.h>
#include <memory>
#include <unistd.h>

#include "exceptions.hpp"

namespace modmqttd {

template <class T>
std::shared_ptr<T>
dll_import(std::string libname, const char* symbol_name)
{
    // Load the shared library
    void* handle = dlopen(libname.c_str(), RTLD_NOW);
    if (!handle) {
        throw ConvPluginNotFoundException("Cannot open library: " + std::string(dlerror()));
    }

    // Load the symbol for the plugin instance
    T* plugin_instance = reinterpret_cast<T*>(dlsym(handle, symbol_name));
    const char* dlsym_error = dlerror();

    if (dlsym_error) {
        dlclose(handle);
        throw ConvPluginNotFoundException("Cannot load symbol '" + std::string(symbol_name) + "': " + std::string(dlsym_error));
    }

    // Create a shared_ptr with a custom deleter
    std::shared_ptr<T> plugin(plugin_instance, [handle](T* ptr) {
        dlclose(handle); // Close the library handle
    });

    return plugin;
}

}
