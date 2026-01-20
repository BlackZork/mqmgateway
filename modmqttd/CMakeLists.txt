set(CMAKE_VERBOSE_MAKEFILE ON)

add_executable(modmqttd 
    main.cpp 
)

#TODO cmake find for mosquitto libraries
target_link_libraries(modmqttd 
    modmqttsrv
    ${MOSQUITTO_LIBRARIES}
    yaml-cpp
    spdlog::spdlog
    fmt::fmt
    ${LIBMODBUS_LIBRARIES} 
    Threads::Threads
    -lpthread
)
