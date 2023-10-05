
#include "plugin.hpp"

#include "bitmask.hpp"
#include "single_arg_ops.hpp"
#include "int16.hpp"
#include "int32.hpp"
#include "scale.hpp"
#include "string.hpp"
#include "uint32.hpp"
#include "float32.hpp"
#include "int8.hpp"

DataConverter*
StdConvPlugin::getConverter(const std::string& name) {
    if (name == "divide")
        return new DivideConverter();
    else if (name == "multiply")
        return new MultiplyConverter();
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
    else if (name == "float32")
        return new FloatConverter();
    else if (name == "int8")
        return new Int8Converter();
    else if (name == "uint8")
        return new UInt8Converter();
    return nullptr;
}
