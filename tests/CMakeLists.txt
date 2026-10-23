# =========================================================================
# 单元测试
# =========================================================================

add_library(doctest INTERFACE)
target_include_directories(doctest INTERFACE
    "${CMAKE_SOURCE_DIR}/third_party/doctest"
)

add_library(icu_core_test STATIC
    "${CMAKE_SOURCE_DIR}/src/core/ProtocolTypes.cpp"
    "${CMAKE_SOURCE_DIR}/src/core/ProtocolRegistry.cpp"
    "${CMAKE_SOURCE_DIR}/src/core/StreamSession.cpp"
    "${CMAKE_SOURCE_DIR}/src/core/SessionManager.cpp"
    "${CMAKE_SOURCE_DIR}/src/config/ConnectionProfile.cpp"
    "${CMAKE_SOURCE_DIR}/src/protocols/tcpip/TcpIpProtocol.cpp"
    "${CMAKE_SOURCE_DIR}/src/protocols/tcpip/TcpTransport.cpp"
    "${CMAKE_SOURCE_DIR}/src/protocols/tcpip/TcpSendStep.cpp"
    "${CMAKE_SOURCE_DIR}/src/protocols/tcpip/UdpTransport.cpp"
    "${CMAKE_SOURCE_DIR}/src/protocols/serial/SerialTransport.cpp"
    "${CMAKE_SOURCE_DIR}/src/protocols/serial/SerialProtocol.cpp"
    "${CMAKE_SOURCE_DIR}/src/protocols/modbus/ModbusReadStep.cpp"
    "${CMAKE_SOURCE_DIR}/src/protocols/modbus/ModbusWriteStep.cpp"
    "${CMAKE_SOURCE_DIR}/src/protocols/modbus/ModbusProtocol.cpp"
    "${CMAKE_SOURCE_DIR}/src/protocols/modbus/ModbusSession.cpp"
    "${CMAKE_SOURCE_DIR}/src/protocols/mqtt/MqttProtocol.cpp"
    "${CMAKE_SOURCE_DIR}/src/protocols/mqtt/MqttSession.cpp"
    "${CMAKE_SOURCE_DIR}/src/protocols/mqtt/MqttBrokerSession.cpp"
    "${CMAKE_SOURCE_DIR}/src/protocols/mqtt/MqttPublishStep.cpp"
)

target_compile_definitions(icu_core_test PUBLIC wxDEBUG_LEVEL=0)

target_include_directories(icu_core_test PUBLIC
    "${CMAKE_SOURCE_DIR}/src"
    "${WX_ROOT}/include"
    "${WX_SETUP_DIR}"
    "${SPDLOG_ROOT}/include"
    "${CMAKE_SOURCE_DIR}/third_party/ModbusLib/src"
)

target_link_directories(icu_core_test PUBLIC
    "${WX_LIB_DIR}"
)

target_link_libraries(icu_core_test PUBLIC
    "${WX_LIB_DIR}/libwx_baseu-3.3.dll.a"
    "${WX_LIB_DIR}/libwx_baseu_net-3.3.dll.a"
    nlohmann_json::nlohmann_json
    sqlite::sqlite
    ws2_32
)

if(ICU_HAS_MODBUSLIB)
    target_link_libraries(icu_core_test PUBLIC modbus)
endif()

if(ICU_HAS_MQTT)
    target_include_directories(icu_core_test PUBLIC
        "${PAHO_C_INSTALL}/include"
        "${PAHO_CPP_INSTALL}/include"
    )
    target_link_libraries(icu_core_test PUBLIC
        "${PAHO_CPP_INSTALL}/lib/libpaho-mqttpp3-static.a"
        "${PAHO_C_INSTALL}/lib/libpaho-mqtt3a-static.a"
        ws2_32 crypt32 rpcrt4
    )
endif()

file(GLOB_RECURSE ICU_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/integration/*.cpp"
)

add_executable(icu_tests
    main.cpp
    ${ICU_TEST_SOURCES}
)

target_include_directories(icu_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

set_target_properties(icu_tests PROPERTIES WIN32_EXECUTABLE FALSE)

target_link_libraries(icu_tests PRIVATE
    icu_core_test
    doctest
)

if(ICU_HAS_MODBUSLIB)
    target_link_libraries(icu_tests PRIVATE modbus)
endif()

if(ICU_HAS_MQTT)
    target_link_libraries(icu_tests PRIVATE
        "${PAHO_CPP_INSTALL}/lib/libpaho-mqttpp3-static.a"
        "${PAHO_C_INSTALL}/lib/libpaho-mqtt3a-static.a"
        ws2_32 crypt32 rpcrt4
    )
endif()

file(GLOB WX_DLLS_TEST "${WX_LIB_DIR}/*.dll")

add_custom_command(TARGET icu_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${WX_DLLS_TEST}
        $<TARGET_FILE_DIR:icu_tests>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:modbus> $<TARGET_FILE_DIR:icu_tests>
    COMMENT "Copying wxWidgets and modbus DLLs to test output directory"
)
add_test(NAME icu_tests COMMAND icu_tests)

if(MINGW)
    target_link_options(icu_tests PRIVATE
        -static-libgcc -static-libstdc++)
endif()

if(ICU_BUILD_COVERAGE AND MINGW)
    target_compile_options(icu_core_test PUBLIC
        --coverage -O0 -g -fprofile-abs-path)
    target_link_options(icu_core_test PUBLIC
        --coverage)

    target_compile_options(icu_tests PRIVATE
        --coverage -O0 -g)
    target_link_options(icu_tests PRIVATE
        --coverage -static-libgcc -static-libstdc++)
endif()

if(ICU_BUILD_COVERAGE AND MINGW)
    find_program(GCOVR_EXECUTABLE gcovr HINTS
        "C:/msys64/ucrt64/bin" "C:/msys64/usr/bin")

    if(GCOVR_EXECUTABLE)
        add_custom_target(coverage
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/coverage"
            COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
            COMMAND ${GCOVR_EXECUTABLE}
                    --root "${CMAKE_SOURCE_DIR}"
                    --filter  ".*/src/.*"
                    --exclude ".*/src/ui/.*"
                    --exclude ".*/src/app/.*"
                    --exclude-unreachable-branches
                    --print-summary
                    --html-details "${CMAKE_BINARY_DIR}/coverage/index.html"
                    --txt         "${CMAKE_BINARY_DIR}/coverage/summary.txt"
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            COMMENT "Generating coverage report"
            VERBATIM
        )
    else()
        message(WARNING "gcovr not found; 'coverage' target disabled")
    endif()
endif()
