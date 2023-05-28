
#include "plugin.hpp"

#include "divide.hpp"
#include "int32.hpp"
#include "bitmask.hpp"
#include "scale.hpp"
#include "int16.hpp"

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
    else if (name == "int16")
        return new Int16Converter();
    return nullptr;
}
