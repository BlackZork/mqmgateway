add_library(stdconv
    MODULE
    argtools.hpp
    bits.hpp
    debug.hpp
    float32.hpp
    float64.hpp
    int8.hpp
    int16.hpp
    int32.hpp
    int64.hpp
    map.cpp
    map.hpp
    plugin.hpp
    plugin.cpp
    scale.hpp
    single_arg_ops.hpp
    string.hpp
    uint32.hpp
    uint64.hpp
)

SET_TARGET_PROPERTIES(stdconv PROPERTIES PREFIX "")