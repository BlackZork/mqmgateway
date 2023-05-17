#include "plugin.hpp"
#include "expr.hpp"

DataConverter*
StdConvPlugin::getConverter(const std::string& name) {
    if(name == "evaluate") {
        return new ExprtkConverter();
    }
    return nullptr;
}
