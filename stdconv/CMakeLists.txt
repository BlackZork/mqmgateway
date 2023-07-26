add_library(stdconv
    MODULE
    bitmask.hpp
    divide.hpp
    float32.hpp
    int16.hpp
    int32.hpp
    plugin.hpp
    plugin.cpp
    scale.hpp
    uint32.hpp
)

SET_TARGET_PROPERTIES(stdconv PROPERTIES PREFIX "")