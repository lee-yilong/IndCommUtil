# =========================================================================
# 单元测试
# =========================================================================

# -------------------------------------------------------------------------
# doctest（header-only，只提供 include 路径）
# -------------------------------------------------------------------------
add_library(doctest INTERFACE)
target_include_directories(doctest INTERFACE
    "${CMAKE_SOURCE_DIR}/third_party/doctest"
)

# -------------------------------------------------------------------------
# icu_core_test：测试用的生产代码静态库
# -------------------------------------------------------------------------
add_library(icu_core_test STATIC
    "${CMAKE_SOURCE_DIR}/src/core/ProtocolTypes.cpp"
    "${CMAKE_SOURCE_DIR}/src/core/ProtocolRegistry.cpp"
    "${CMAKE_SOURCE_DIR}/src/core/StreamSession.cpp"
    "${CMAKE_SOURCE_DIR}/src/core/SessionManager.cpp"
    "${CMAKE_SOURCE_DIR}/src/config/ConnectionProfile.cpp"
    "${CMAKE_SOURCE_DIR}/src/protocols/tcp/TcpProtocol.cpp"
    "${CMAKE_SOURCE_DIR}/src/protocols/tcp/TcpTransport.cpp"
    "${CMAKE_SOURCE_DIR}/src/protocols/serial/SerialTransport.cpp"
    "${CMAKE_SOURCE_DIR}/src/protocols/serial/SerialProtocol.cpp"

    # 如果有 Modbus 的实现文件，也加进来：
    "${CMAKE_SOURCE_DIR}/src/protocols/modbus/ModbusProtocol.cpp"
    "${CMAKE_SOURCE_DIR}/src/protocols/modbus/ModbusSession.cpp"
)

# 同 CMakeLists.txt：强制 wxDEBUG_LEVEL=0
# PUBLIC 让 icu_tests 也继承，避免 TcpTransport.cpp 引用 wxOnAssert
target_compile_definitions(icu_core_test PUBLIC wxDEBUG_LEVEL=0)

target_include_directories(icu_core_test PUBLIC
    "${CMAKE_SOURCE_DIR}/src"
    "${WX_ROOT}/include"
    "${WX_SETUP_DIR}"                     # ← 原来是 "${WX_ROOT}/lib/gcc_dll/${WX_SETUP_DIR}"
    "${SPDLOG_ROOT}/include"
    "${CMAKE_SOURCE_DIR}/third_party/ModbusLib/src"
)

target_link_directories(icu_core_test PUBLIC
    "${WX_LIB_DIR}"                       # ← 原来是 "${WX_ROOT}/lib/gcc_dll"
)

target_link_libraries(icu_core_test PUBLIC
    "${WX_LIB_DIR}/libwx_baseu-3.3.dll.a"
    "${WX_LIB_DIR}/libwx_baseu_net-3.3.dll.a"
    nlohmann_json::nlohmann_json
    ws2_32
)

# -------------------------------------------------------------------------
# icu_tests：测试可执行文件
# -------------------------------------------------------------------------
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
    modbus
)

# 复制 wxWidgets DLL 到 build/tests/
# 不做 build type 过滤，全量复制。理由：
#   icu_tests 链接的是 release 导入库（libwx_mswu_*，不带 d），
#   运行时需要 release DLL；过滤逻辑在 Debug 构建下只复制带 d 的，
#   会导致 STATUS_DLL_NOT_FOUND (0xc0000135)。
file(GLOB WX_DLLS_TEST "${WX_LIB_DIR}/*.dll")

add_custom_command(TARGET icu_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${WX_DLLS_TEST}
        $<TARGET_FILE_DIR:icu_tests>
    COMMENT "Copying wxWidgets DLLs to test output directory"
)
add_test(NAME icu_tests COMMAND icu_tests)

# -------------------------------------------------------------------------
# 静态链接 MinGW 运行时
# -------------------------------------------------------------------------
if(MINGW)
    target_link_options(icu_tests PRIVATE
        -static-libgcc -static-libstdc++)
endif()

# -------------------------------------------------------------------------
# 覆盖率插桩
# -------------------------------------------------------------------------
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

# -------------------------------------------------------------------------
# coverage target
# -------------------------------------------------------------------------
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
