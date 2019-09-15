add_library(stdconv
    MODULE
    bitmask.hpp
    divide.hpp
    int32.hpp
    plugin.hpp
    plugin.cpp
)

SET_TARGET_PROPERTIES(stdconv PROPERTIES PREFIX "")