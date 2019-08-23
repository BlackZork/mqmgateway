find_package(Catch2 REQUIRED)

add_executable(tests
    defaults.hpp
    device_template_tests.cpp
    converter_name_parser_tests.cpp
    jsonutils.cpp
    jsonutils.hpp
    main.cpp
    mockedmodbuscontext.cpp
    mockedmodbuscontext.hpp
    mockedmqttimpl.cpp
    mockedmqttimpl.hpp
    mockedserver.hpp
    # tests
    mqtt_named_list_conv_tests.cpp
    mqtt_named_list_tests.cpp
    mqtt_named_scalar_conv_tests.cpp
    mqtt_register_default_slave_tests.cpp
    mqtt_state_map_conv_tests.cpp
    mqtt_unnamed_list_conv_tests.cpp
    mqtt_unnamed_list_tests.cpp
    mqtt_unnamed_scalar_conv_tests.cpp
    mqtt_unnamed_scalar_tests.cpp
    real_server_tests.cpp
    scheduler_tests.cpp
    single_register_noavail_tests.cpp
    single_register_tests.cpp
    two_slaves_tests.cpp
)

file(COPY test_templates DESTINATION .)

target_link_libraries(tests 
    modmqttsrv
    mosquittopp 
    mosquitto 
    Catch2::Catch2
    ${YAML_CPP_LIBRARIES} 
    ${Boost_LIBRARIES} 
    ${LIBMODBUS_LIBRARIES} 
    ${CMAKE_THREAD_LIBS_INIT}
)
