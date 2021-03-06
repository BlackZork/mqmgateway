find_package(Catch2 REQUIRED)

add_executable(tests
    defaults.hpp
    jsonutils.cpp
    jsonutils.hpp
    main.cpp
    mockedmodbuscontext.cpp
    mockedmodbuscontext.hpp
    mockedmqttimpl.cpp
    mockedmqttimpl.hpp
    mockedserver.hpp
    # tests
    converter_name_parser_tests.cpp
    exprconv_tests.cpp
    mqtt_command_tests.cpp
    mqtt_named_list_conv_tests.cpp
    mqtt_named_list_tests.cpp
    mqtt_named_scalar_conv_tests.cpp
    mqtt_register_default_slave_tests.cpp
    mqtt_state_map_conv_tests.cpp
    mqtt_state_map_tests.cpp
    mqtt_unnamed_list_conv_tests.cpp
    mqtt_unnamed_list_expr_tests.cpp
    mqtt_unnamed_list_tests.cpp
    mqtt_unnamed_scalar_conv_tests.cpp
    mqtt_unnamed_scalar_tests.cpp
    real_server_tests.cpp
    scheduler_tests.cpp
    single_register_noavail_tests.cpp
    single_register_tests.cpp
    stdconv_tests.cpp
    two_slaves_tests.cpp
)

target_link_libraries(tests 
    modmqttsrv
    mosquittopp 
    mosquitto 
    Catch2::Catch2
    ${YAML_CPP_LIBRARIES} 
    ${Boost_LIBRARIES} 
    ${LIBMODBUS_LIBRARIES} 
    ${CMAKE_THREAD_LIBS_INIT}
    ${CMAKE_DL_LIBS}    
)
