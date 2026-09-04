#include "plugin.hpp"
#include "expr.hpp"

extern "C" const int converter_plugin_abi_version = CONVERTER_ABI_VERSION;

DataConverter*
ExprConvPlugin::getConverter(const std::string& pName) {
    if (pName == "evaluate") {
        return new ExprtkConverter();
    }
    return nullptr;
}
