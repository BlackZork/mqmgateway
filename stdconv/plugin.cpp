
#include "plugin.hpp"

#include "divide.hpp"
#include "int32.hpp"

IStateConverter*
StdConvPlugin::getStateConverter(const std::string& name) {
    if (name == "divide")
        return new DivideConverter();
    else if (name == "int32")
        return new Int32Converter();
    return nullptr;
}
