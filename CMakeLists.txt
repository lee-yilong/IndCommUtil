cmake_minimum_required(VERSION 3.20)

project(IndustrialCommunicationUtility
    VERSION 0.1.0
    DESCRIPTION "工业通信实用工具（多协议通讯测试/调试工具）"
    HOMEPAGE_URL "https://github.com/lee-yilong/IndCommUtil"
    LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
endif()

# =========================================================================
# nlohmann/json（header-only，通过 CMake target 引入）
# =========================================================================
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")
add_subdirectory(third_party/json)

set(MODBUSLIB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ModbusLib")
if(EXISTS "${MODBUSLIB_ROOT}/CMakeLists.txt")
    add_subdirectory(${MODBUSLIB_ROOT} ${CMAKE_CURRENT_BINARY_DIR}/ModbusLib)
    set(ICU_HAS_MODBUSLIB ON)
else()
    message(WARNING "ModbusLib not found; Modbus protocol disabled")
    set(ICU_HAS_MODBUSLIB OFF)
endif()

add_executable(IndustrialCommunicationUtility WIN32
    ${APP_SOURCES}
    ${APP_HEADERS}
    resources/app.rc
)

# wx 3.3 在非 NDEBUG 构建下默认 wxDEBUG_LEVEL=1，
# 会让 wxASSERT 展开成对 wxOnAssert 的引用（只在 debug 库里有）。
# 项目链接的是 release wx 库，需要强制 wxDEBUG_LEVEL=0 才能链接。
# 不影响项目自己的 assert（<cassert> 依赖 NDEBUG，与此无关）。
target_compile_definitions(IndustrialCommunicationUtility PRIVATE wxDEBUG_LEVEL=0)

# =========================================================================
# 头文件搜索路径
# =========================================================================
target_include_directories(IndustrialCommunicationUtility PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
    "${WX_ROOT}/include"
    "${WX_SETUP_DIR}"
    "${SPDLOG_ROOT}/include"
)

target_link_directories(IndustrialCommunicationUtility PRIVATE
    "${WX_LIB_DIR}"
)

# =========================================================================
# 链接 wxWidgets 与系统库
# =========================================================================
target_link_libraries(IndustrialCommunicationUtility PRIVATE
    "${WX_LIB_DIR}/libwx_mswu_core-3.3.dll.a"
    "${WX_LIB_DIR}/libwx_baseu-3.3.dll.a"
    "${WX_LIB_DIR}/libwx_baseu_net-3.3.dll.a"

    nlohmann_json::nlohmann_json

    kernel32 user32 gdi32 comdlg32 winspool winmm
    shell32 ole32 oleaut32 uuid comctl32 rpcrt4
    wsock32 odbc32 ws2_32
)

# =========================================================================
# MinGW 编码与静态运行时
# =========================================================================
if(MINGW)
    target_compile_options(IndustrialCommunicationUtility PRIVATE
        -finput-charset=UTF-8 -fexec-charset=UTF-8)
    target_link_options(IndustrialCommunicationUtility PRIVATE
        -static-libgcc -static-libstdc++ -static)
endif()

# =========================================================================
# 构建后复制 wxWidgets DLL
# =========================================================================
file(GLOB WX_DLLS "${WX_LIB_DIR}/*.dll")

add_custom_command(TARGET IndustrialCommunicationUtility POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${WX_DLLS}
        $<TARGET_FILE_DIR:IndustrialCommunicationUtility>
    COMMENT "Copying wxWidgets DLLs to output directory"
)

# =========================================================================
# i18n：每次构建都重新编译 .po -> .mo
# =========================================================================
find_program(GETTEXT_XGETTEXT xgettext HINTS
    "C:/msys64/ucrt64/bin" "C:/msys64/usr/bin")
find_program(GETTEXT_MSGMERGE msgmerge HINTS
    "C:/msys64/ucrt64/bin" "C:/msys64/usr/bin")
find_program(GETTEXT_MSGFMT   msgfmt   HINTS
    "C:/msys64/ucrt64/bin" "C:/msys64/usr/bin")

set(I18N_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/resources/i18n")
set(POT_FILE    "${I18N_SRC_DIR}/IndustrialCommunicationUtility.pot")
set(I18N_LANGS  zh_CN en_US)

if(GETTEXT_XGETTEXT AND GETTEXT_MSGMERGE AND GETTEXT_MSGFMT)

    # ---------- 收集相对路径的源文件（含 .h，因为 header-only Codec 里有 _()） ----------
    file(GLOB_RECURSE I18N_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    )

    set(I18N_SRC_REL "")
    foreach(f ${I18N_SOURCES})
        file(RELATIVE_PATH rel "${CMAKE_CURRENT_SOURCE_DIR}" "${f}")
        list(APPEND I18N_SRC_REL "${rel}")
    endforeach()

    # ---------- i18n_update 目标：提取 + 合并 + 编译 ----------
    add_custom_target(i18n_update ALL
        COMMAND ${CMAKE_COMMAND} -E chdir "${CMAKE_CURRENT_SOURCE_DIR}"
                "${GETTEXT_XGETTEXT}"
                --language=C++
                --keyword=_
                --keyword=wxTRANSLATE
                --from-code=UTF-8
                -o "${POT_FILE}"
                ${I18N_SRC_REL}
        COMMENT "Extracting i18n strings (relative paths)"
        VERBATIM
    )

    foreach(lang ${I18N_LANGS})
        set(PO_FILE "${I18N_SRC_DIR}/${lang}/LC_MESSAGES/IndustrialCommunicationUtility.po")
        set(MO_FILE "${I18N_SRC_DIR}/${lang}/LC_MESSAGES/IndustrialCommunicationUtility.mo")

        add_custom_command(TARGET i18n_update POST_BUILD
            COMMAND "${GETTEXT_MSGMERGE}"
                    --update --backup=none --quiet
                    "${PO_FILE}" "${POT_FILE}"
            COMMAND "${GETTEXT_MSGFMT}"
                    --check-format
                    -o "${MO_FILE}"
                    "${PO_FILE}"
            COMMENT "Merging & compiling translation for ${lang}"
            VERBATIM
        )
    endforeach()

    add_dependencies(IndustrialCommunicationUtility i18n_update)

    add_custom_command(TARGET IndustrialCommunicationUtility POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${I18N_SRC_DIR}"
            "$<TARGET_FILE_DIR:IndustrialCommunicationUtility>/resources/i18n"
        COMMENT "Copying i18n .mo files"
    )

else()
    message(WARNING "gettext tools not found; i18n disabled")
endif()

# =========================================================================
# 测试目标
# =========================================================================
if(ICU_BUILD_TESTS)
    add_subdirectory(tests)
endif()

# =========================================================================
# 覆盖率（仅当 ICU_BUILD_COVERAGE=ON 且 GCC/MinGW）
# =========================================================================
if(ICU_BUILD_COVERAGE AND MINGW)
    message(STATUS "Coverage instrumentation enabled (GCC --coverage)")
endif()

if(ICU_HAS_MODBUSLIB)
    # 1. 把 ModbusLib 的 src 目录加入头文件搜索路径
    target_include_directories(IndustrialCommunicationUtility PRIVATE "${MODBUSLIB_ROOT}/src")

    # 2. 链接库
    target_link_libraries(IndustrialCommunicationUtility PRIVATE modbus)

    # 3. 拷贝 DLL
    add_custom_command(TARGET IndustrialCommunicationUtility POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:modbus>
            $<TARGET_FILE_DIR:IndustrialCommunicationUtility>
        COMMENT "Copying ModbusLib DLL to output directory"
    )
endif()
