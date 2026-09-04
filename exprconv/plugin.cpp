#include "plugin.hpp"
#include "expr.hpp"

extern "C" const int converter_plugin_abi_version = CONVERTER_ABI_VERSION;

DataConverter*
StdConvPlugin::getConverter(const std::string& name) {
    if(name == "evaluate") {
        return new ExprtkConverter();
    }
    return nullptr;
}
