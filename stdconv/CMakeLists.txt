add_library(stdconv
    MODULE
    divide.hpp
    plugin.hpp
    plugin.cpp
)

SET_TARGET_PROPERTIES(stdconv PROPERTIES PREFIX "")