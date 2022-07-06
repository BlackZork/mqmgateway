set(CMAKE_VERBOSE_MAKEFILE ON)

add_executable(modmqttd 
    main.cpp 
)

find_package(yaml-cpp REQUIRED)


#TODO cmake find for mosquitto libraries
target_link_libraries(modmqttd 
    modmqttsrv
    mosquitto 
    yaml-cpp
    ${Boost_LIBRARIES} 
    ${LIBMODBUS_LIBRARIES} 
    ${CMAKE_THREAD_LIBS_INIT}
)
