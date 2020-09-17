#include "plugin.hpp"
#include "expr.hpp"

IStateConverter*
StdConvPlugin::getStateConverter(const std::string& name) {
    if(name == "evaluate") {
        return new ExprtkConverter();
    }
    return nullptr;
}
