add_library(badabiconv
    MODULE
    plugin.cpp
)

add_library(noabiconv
    MODULE
    plugin.cpp
)
target_compile_definitions(noabiconv PRIVATE OMIT_ABI_VERSION)

SET_TARGET_PROPERTIES(badabiconv noabiconv PROPERTIES PREFIX "")
