
#include "plugin.hpp"

#include "bitmask.hpp"
#include "divide.hpp"
#include "int16.hpp"
#include "int32.hpp"
#include "scale.hpp"
#include "string.hpp"
#include "uint32.hpp"

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
    else if (name == "string")
        return new StringConverter();
    else if (name == "int16")
        return new Int16Converter();
    else if (name == "uint32")
        return new UInt32Converter();
    return nullptr;
}
