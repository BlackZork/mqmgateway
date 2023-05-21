
#include "plugin.hpp"

#include "divide.hpp"
#include "int32.hpp"
#include "bitmask.hpp"
#include "scale.hpp"

DataConverter*
StdConvPlugin::getConverter(const std::string& name) {
    if (name == "divide")
        return new DivideConverter();
    else if (name == "int32")
        return new Int32Converter();
    else if (name == "bitmask")
        return new BitmaskConverter();
    else if (name == "scale")
        return new ScaleConverter();
    return nullptr;
}
