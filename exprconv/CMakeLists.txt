add_library(exprconv
    MODULE
    plugin.hpp
    plugin.cpp
)

SET_TARGET_PROPERTIES(exprconv PROPERTIES PREFIX "")