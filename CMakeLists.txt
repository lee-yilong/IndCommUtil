cmake_minimum_required(VERSION 3.20)

project(IndCommUtil
    VERSION 0.1.0
    DESCRIPTION "工业通信实用工具（多协议通讯测试/调试工具）"
    HOMEPAGE_URL "https://github.com/lee-yilong/IndCommUtil"
    LANGUAGES C CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(I18N_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/resources/i18n")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/version.h"
    @ONLY
)

set(WX_ROOT     "${CMAKE_CURRENT_SOURCE_DIR}/third_party/wxWidgets")
set(SPDLOG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/spdlog")
set(JSON_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/json")

if(EXISTS "${WX_ROOT}/lib/gcc_dll/libwx_mswu_core-3.3.dll.a")
    set(WX_LIB_DIR   "${WX_ROOT}/lib/gcc_dll")
    set(WX_SETUP_DIR "${WX_ROOT}/lib/gcc_dll/mswu")
    message(STATUS "wxWidgets layout: legacy (lib/gcc_dll)")
elseif(EXISTS "${WX_ROOT}/lib/libwx_mswu_core-3.3.dll.a")
    set(WX_LIB_DIR   "${WX_ROOT}/lib")
    set(WX_SETUP_DIR "${WX_ROOT}/lib/wx/include/msw-unicode-3.3")
    message(STATUS "wxWidgets layout: standard (lib/)")
else()
    message(FATAL_ERROR
        "wxWidgets libraries not found. Looked in:\n"
        "  ${WX_ROOT}/lib/gcc_dll/libwx_mswu_core-3.3.dll.a\n"
        "  ${WX_ROOT}/lib/libwx_mswu_core-3.3.dll.a\n"
        "See docs/developer/building.md")
endif()

set(PAHO_C_INSTALL "${CMAKE_CURRENT_SOURCE_DIR}/third_party/paho.mqtt.c/build/install")
set(PAHO_CPP_INSTALL "${CMAKE_CURRENT_SOURCE_DIR}/third_party/paho.mqtt.cpp/build/install")

if(EXISTS "${PAHO_C_INSTALL}/lib/libpaho-mqtt3a-static.a" AND EXISTS "${PAHO_CPP_INSTALL}/lib/libpaho-mqttpp3-static.a")
    set(ICU_HAS_MQTT ON)
    message(STATUS "Paho MQTT: found pre-compiled static libraries")
else()
    message(WARNING "Paho MQTT: static libraries not found! Run scripts/build_paho.sh first.")
    set(ICU_HAS_MQTT OFF)
endif()

file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

file(GLOB_RECURSE APP_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")

option(ICU_BUILD_TESTS "Build unit/integration tests" OFF)
option(ICU_BUILD_COVERAGE "Enable code coverage instrumentation" OFF)

set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")
add_subdirectory(third_party/json)

add_subdirectory(third_party/sqlite)

set(MODBUSLIB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ModbusLib")
if(EXISTS "${MODBUSLIB_ROOT}/CMakeLists.txt")
    set(MB_BUILD_EXAMPLES OFF CACHE BOOL "Disable ModbusLib examples" FORCE)
    add_subdirectory(${MODBUSLIB_ROOT} ${CMAKE_CURRENT_BINARY_DIR}/ModbusLib EXCLUDE_FROM_ALL)
    set(ICU_HAS_MODBUSLIB ON)
else()
    message(WARNING "ModbusLib not found; Modbus protocol disabled")
    set(ICU_HAS_MODBUSLIB OFF)
endif()

file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/protocols/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/config/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/log/*.cpp"
)

add_library(icu_core_lib STATIC ${CORE_SOURCES})

target_compile_definitions(icu_core_lib PRIVATE wxDEBUG_LEVEL=0 NO_GUI_SINK)
target_include_directories(icu_core_lib PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
    "${WX_ROOT}/include"
    "${WX_SETUP_DIR}"
    "${SPDLOG_ROOT}/include"
    "${MODBUSLIB_ROOT}/src"
)

if(ICU_HAS_MQTT)
    target_include_directories(icu_core_lib PUBLIC
        "${PAHO_C_INSTALL}/include"
        "${PAHO_CPP_INSTALL}/include"
    )
endif()

target_link_directories(icu_core_lib PUBLIC "${WX_LIB_DIR}")

target_link_libraries(icu_core_lib PUBLIC
    "${WX_LIB_DIR}/libwx_baseu-3.3.dll.a"
    "${WX_LIB_DIR}/libwx_baseu_net-3.3.dll.a"
    nlohmann_json::nlohmann_json
    sqlite::sqlite
    ws2_32
)

if(ICU_HAS_MODBUSLIB)
    target_link_libraries(icu_core_lib PUBLIC modbus)
endif()

if(ICU_HAS_MQTT)
    target_link_libraries(icu_core_lib PUBLIC
        "${PAHO_CPP_INSTALL}/lib/libpaho-mqttpp3-static.a"
        "${PAHO_C_INSTALL}/lib/libpaho-mqtt3a-static.a"
        ws2_32 crypt32 rpcrt4
    )
endif()

file(GLOB_RECURSE GUI_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/app/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/settings/*.cpp"
)

add_executable(IndComm WIN32
    src/main.cpp
    ${GUI_SOURCES}
    ${APP_HEADERS}
    resources/app.rc
)

target_compile_definitions(IndComm PRIVATE wxDEBUG_LEVEL=0)
target_link_libraries(IndComm PRIVATE
    icu_core_lib
    "${WX_LIB_DIR}/libwx_mswu_core-3.3.dll.a"
    "${WX_LIB_DIR}/libwx_baseu-3.3.dll.a"
    "${WX_LIB_DIR}/libwx_baseu_net-3.3.dll.a"
    kernel32 user32 gdi32 comdlg32 winspool winmm
    shell32 ole32 oleaut32 uuid comctl32 rpcrt4
)

target_compile_definitions(IndComm PRIVATE ICU_BUILD_GUI)

add_executable(indcomm-cli
    src/cli/main.cpp
)

target_link_libraries(indcomm-cli PRIVATE icu_core_lib)

file(GLOB WX_DLLS "${WX_LIB_DIR}/*.dll")

add_custom_command(TARGET IndComm POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${WX_DLLS} $<TARGET_FILE_DIR:IndComm>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:modbus> $<TARGET_FILE_DIR:IndComm>
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:IndComm>/resources/i18n"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/resources/i18n" "$<TARGET_FILE_DIR:IndComm>/resources/i18n"
    COMMENT "Copying DLLs and i18n to GUI output"
)

add_custom_command(TARGET indcomm-cli POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${WX_DLLS} $<TARGET_FILE_DIR:indcomm-cli>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:modbus> $<TARGET_FILE_DIR:indcomm-cli>
    COMMENT "Copying DLLs to CLI output"
)

# ★ 修复：把测试目录放到最后，确保所有 target（如 modbus）都已生成
if(ICU_BUILD_TESTS)
    enable_testing()
    include(CTest)
    add_subdirectory(tests)
endif()
