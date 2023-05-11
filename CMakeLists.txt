cmake_minimum_required(VERSION 3.13)
project(modmqttd VERSION 1.0.0)

option(WITHOUT_TESTS "Do not build unit tests")
set(build_unittests)
if (WITHOUT_TESTS)
    set(build_unittests EXCLUDE_FROM_ALL)
endif()

find_package(Boost 1.54.0 REQUIRED COMPONENTS log program_options)
set(Boost_USE_STATIC_LIBS        OFF)
set(Boost_USE_MULTITHREADED      ON)
set(Boost_USE_STATIC_RUNTIME     OFF)
set(BOOST_ALL_DYN_LINK           ON) 

find_package(PkgConfig)
pkg_check_modules(LIBMODBUS REQUIRED libmodbus)
pkg_check_modules(LIBMOSQUITTO REQUIRED libmosquitto>=2.0.0)

find_package(Threads)

find_package(yaml-cpp REQUIRED)
find_package(RapidJSON REQUIRED)

find_path(EXPRTK_INCLUDE_DIR "exprtk.hpp")
message(STATUS "EXPRTK_INCLUDE_DIR => ${EXPRTK_INCLUDE_DIR}")

include_directories(${Boost_INCLUDE_DIRS} ${YAML_CPP_INCLUDE_DIR} ${RAPIDJSON_INCLUDE_DIRS} .)
link_directories(${Boost_LIBRARY_DIRS})

# force boost dynamic linking
add_definitions(-DBOOST_LOG_DYN_LINK)

add_subdirectory(libmodmqttconv)
add_subdirectory(libmodmqttsrv)
add_subdirectory(modmqttd)
add_subdirectory(unittests ${build_unittests})


set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib/modmqttd")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

add_subdirectory(stdconv)
add_dependencies(modmqttd stdconv)
add_dependencies(tests stdconv)

if (EXPRTK_INCLUDE_DIR)
    include_directories(${EXPRTK_INCLUDE_DIR})
    add_subdirectory(exprconv)    
    target_compile_definitions(tests PRIVATE HAVE_EXPRTK)

    add_dependencies(modmqttd exprconv)
    add_dependencies(tests exprconv)

    install(TARGETS exprconv DESTINATION lib/modmqttd)
endif()



install (TARGETS modmqttd DESTINATION bin)

install (TARGETS modmqttsrv DESTINATION lib)
install (DIRECTORY "${PROJECT_SOURCE_DIR}/libmodmqttconv" DESTINATION include FILES_MATCHING PATTERN "*.hpp")

install(TARGETS stdconv DESTINATION lib/modmqttd)

install(FILES modmqttd.service DESTINATION lib/systemd/system) 