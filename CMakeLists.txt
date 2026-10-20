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

# =========================================================================
# 版本信息（cmake/version.h.in → <build>/generated/version.h）
# =========================================================================
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/version.h"
    @ONLY
)

set(WX_ROOT     "${CMAKE_CURRENT_SOURCE_DIR}/third_party/wxWidgets")
set(SPDLOG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/spdlog")
set(JSON_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/json")

# -------------------------------------------------------------------------
# 探测 wxWidgets 布局：wx 3.3 有两种
#   老式（in-tree build）：lib/gcc_dll/libwx_mswu_core-3.3.dll.a
#                         lib/gcc_dll/mswu/wx/setup.h
#   新式（std layout）：   lib/libwx_mswu_core-3.3.dll.a
#                         lib/wx/include/msw-unicode-3.3/wx/setup.h
# -------------------------------------------------------------------------
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

# =========================================================================
# 源文件：递归扫描 src/ 下所有 .cpp 和 .h
# 使用 CONFIGURE_DEPENDS 让 CMake 每次构建都重新检查文件变化
# =========================================================================
file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

file(GLOB_RECURSE APP_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")

# =========================================================================
# 测试开关
# =========================================================================
option(ICU_BUILD_TESTS "Build unit/integration tests" OFF)
option(ICU_BUILD_COVERAGE "Enable code coverage instrumentation" OFF)

if(ICU_BUILD_TESTS)
    enable_testing()
    include(CTest)
    add_subdirectory(tests)
endif()

# =========================================================================
# nlohmann/json（header-only，通过 CMake target 引入）
# =========================================================================
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")
add_subdirectory(third_party/json)
# SQLite (amalgamation)
add_subdirectory(third_party/sqlite)

set(MODBUSLIB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ModbusLib")
if(EXISTS "${MODBUSLIB_ROOT}/CMakeLists.txt")
    # 在引入 ModbusLib 之前，关掉它的 examples 构建
    set(MB_BUILD_EXAMPLES OFF CACHE BOOL "Disable ModbusLib examples" FORCE)
    add_subdirectory(${MODBUSLIB_ROOT} ${CMAKE_CURRENT_BINARY_DIR}/ModbusLib EXCLUDE_FROM_ALL)
    set(ICU_HAS_MODBUSLIB ON)
else()
    message(WARNING "ModbusLib not found; Modbus protocol disabled")
    set(ICU_HAS_MODBUSLIB OFF)
endif()

# =========================================================================
# 核心静态库（GUI 与 CLI 共用）
# =========================================================================
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

target_link_directories(icu_core_lib PUBLIC "${WX_LIB_DIR}")

# 核心库只链接基础依赖，不链接 GUI 专用的 wx 库
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

# =========================================================================
# GUI 主程序 (IndComm.exe)
# =========================================================================
file(GLOB_RECURSE GUI_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/app/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/settings/*.cpp"
)

# ★ 修改目标名称为 IndComm
add_executable(IndComm WIN32
    src/main.cpp
    ${GUI_SOURCES}
    ${APP_HEADERS}
    resources/app.rc
)

target_compile_definitions(IndComm PRIVATE wxDEBUG_LEVEL=0)
target_link_libraries(IndComm PRIVATE
    icu_core_lib
    # 必须放在系统库前面，确保符号解析顺序
    "${WX_LIB_DIR}/libwx_mswu_core-3.3.dll.a"
    "${WX_LIB_DIR}/libwx_baseu-3.3.dll.a"
    "${WX_LIB_DIR}/libwx_baseu_net-3.3.dll.a"

    kernel32 user32 gdi32 comdlg32 winspool winmm
    shell32 ole32 oleaut32 uuid comctl32 rpcrt4
)

# GUI 专属链接
target_link_libraries(IndComm PRIVATE
    kernel32 user32 gdi32 comdlg32 winspool winmm
    shell32 ole32 oleaut32 uuid comctl32 rpcrt4
)

# =========================================================================
# CLI 命令行工具 (indcomm-cli.exe)
# =========================================================================
# ★ 修改目标名称为 indcomm-cli
add_executable(indcomm-cli
    src/cli/main.cpp
)

target_link_libraries(indcomm-cli PRIVATE icu_core_lib)
target_compile_definitions(icu_core_lib PRIVATE)
target_compile_definitions(IndComm PRIVATE ICU_BUILD_GUI)

# =========================================================================
# 复制依赖 DLL 及 i18n 资源
# =========================================================================
file(GLOB WX_DLLS "${WX_LIB_DIR}/*.dll")

# ★ 把目标替换为 IndComm
# 确保 i18n 目录存在，如果不存在就创建一个（防止空目录报错）
add_custom_command(TARGET IndComm POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${WX_DLLS} $<TARGET_FILE_DIR:IndComm>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:modbus> $<TARGET_FILE_DIR:IndComm>
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:IndComm>/resources/i18n"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/resources/i18n" "$<TARGET_FILE_DIR:IndComm>/resources/i18n"
    COMMENT "Copying DLLs and i18n to GUI output"
)

# ★ 把目标替换为 indcomm-cli
add_custom_command(TARGET indcomm-cli POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${WX_DLLS} $<TARGET_FILE_DIR:indcomm-cli>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:modbus> $<TARGET_FILE_DIR:indcomm-cli>
    COMMENT "Copying DLLs to CLI output"
)
