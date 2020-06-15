add_library(stdconv
    MODULE
    bitmask.hpp
    divide.hpp
    int32.hpp
    plugin.hpp
    plugin.cpp
    scale.hpp
)

SET_TARGET_PROPERTIES(stdconv PROPERTIES PREFIX "")