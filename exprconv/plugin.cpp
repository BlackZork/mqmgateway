#include "plugin.hpp"
#include "expr.hpp"

IStateConverter*
StdConvPlugin::getStateConverter(const std::string& name) {
    if(name == "expr") {
        return new ExprtkConverter();
    }
    return nullptr;
}
