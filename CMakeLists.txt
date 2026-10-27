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

# =========================================================================
# 1. 资源文件配置
# =========================================================================
set(APP_ICO "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/app.ico")
if(NOT EXISTS "${APP_ICO}")
    message(FATAL_ERROR "错误：找不到 ${APP_ICO}，请把图标放到该目录下！")
endif()

set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -I\"${CMAKE_CURRENT_SOURCE_DIR}/resources\"")

# =========================================================================
# 2. 依赖与源文件配置
# =========================================================================
set(WX_ROOT     "${CMAKE_CURRENT_SOURCE_DIR}/third_party/wxWidgets")
set(SPDLOG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/spdlog")
set(JSON_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/json")

if(EXISTS "${WX_ROOT}/lib/gcc_dll/libwx_mswu_core-3.3.dll.a")
    set(WX_LIB_DIR   "${WX_ROOT}/lib/gcc_dll")
    set(WX_SETUP_DIR "${WX_ROOT}/lib/gcc_dll/mswu")
elseif(EXISTS "${WX_ROOT}/lib/libwx_mswu_core-3.3.dll.a")
    set(WX_LIB_DIR   "${WX_ROOT}/lib")
    set(WX_SETUP_DIR "${WX_ROOT}/lib/wx/include/msw-unicode-3.3")
else()
    message(FATAL_ERROR "wxWidgets libraries not found.")
endif()

set(PAHO_C_INSTALL "${CMAKE_CURRENT_SOURCE_DIR}/third_party/paho.mqtt.c/build/install")
set(PAHO_CPP_INSTALL "${CMAKE_CURRENT_SOURCE_DIR}/third_party/paho.mqtt.cpp/build/install")

if(EXISTS "${PAHO_C_INSTALL}/lib/libpaho-mqtt3a-static.a" AND EXISTS "${PAHO_CPP_INSTALL}/lib/libpaho-mqttpp3-static.a")
    set(ICU_HAS_MQTT ON)
else()
    set(ICU_HAS_MQTT OFF)
endif()

file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE APP_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")

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
    target_include_directories(icu_core_lib PUBLIC "${PAHO_C_INSTALL}/include" "${PAHO_CPP_INSTALL}/include")
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

# ★ 显式确保 WelcomePanel 被打包
list(APPEND GUI_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/WelcomePanel.cpp")
list(REMOVE_DUPLICATES GUI_SOURCES)

add_executable(IndComm WIN32
    src/main.cpp
    ${GUI_SOURCES}
    ${APP_HEADERS}
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc"
)

target_compile_definitions(IndComm PRIVATE wxDEBUG_LEVEL=0)
target_link_libraries(IndComm PRIVATE
    icu_core_lib
    "${WX_LIB_DIR}/libwx_mswu_core-3.3.dll.a"
    "${WX_LIB_DIR}/libwx_mswu_aui-3.3.dll.a"
    "${WX_LIB_DIR}/libwx_baseu-3.3.dll.a"
    "${WX_LIB_DIR}/libwx_baseu_net-3.3.dll.a"
    kernel32 user32 gdi32 comdlg32 winspool winmm
    shell32 ole32 oleaut32 uuid comctl32 rpcrt4
)

target_compile_definitions(IndComm PRIVATE ICU_BUILD_GUI)

add_executable(indcomm-cli src/cli/main.cpp)
target_link_libraries(indcomm-cli PRIVATE icu_core_lib)

if(MINGW)
    target_link_options(IndComm PRIVATE -static-libgcc -static-libstdc++ -static)
    target_link_options(indcomm-cli PRIVATE -static-libgcc -static-libstdc++ -static)
endif()

# =========================================================================
# i18n：构建时从 .po 生成 .mo
#   仓库只跟踪 .po（.pot/.mo 被 .gitignore 忽略），因此安装/运行所需的 .mo
#   必须在构建时生成；本机没有 msgfmt 时退回复制源目录中的 .mo。
# =========================================================================
find_program(GETTEXT_MSGFMT_EXECUTABLE msgfmt)
set(I18N_LANGUAGES zh_CN en_US)
set(I18N_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/resources/i18n")
set(I18N_MO_FILES "")
set(I18N_COPY_COMMANDS "")

if(GETTEXT_MSGFMT_EXECUTABLE)
    foreach(lang ${I18N_LANGUAGES})
        set(_po "${I18N_SRC_DIR}/${lang}/LC_MESSAGES/IndustrialCommunicationUtility.po")
        set(_mo "${I18N_BUILD_DIR}/${lang}/LC_MESSAGES/IndustrialCommunicationUtility.mo")
        add_custom_command(
            OUTPUT "${_mo}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${I18N_BUILD_DIR}/${lang}/LC_MESSAGES"
            COMMAND "${GETTEXT_MSGFMT_EXECUTABLE}" --check-format -o "${_mo}" "${_po}"
            DEPENDS "${_po}"
            COMMENT "Compiling translation ${lang}"
            VERBATIM
        )
        list(APPEND I18N_MO_FILES "${_mo}")
        list(APPEND I18N_COPY_COMMANDS
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:IndComm>/resources/i18n/${lang}/LC_MESSAGES"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_mo}" "$<TARGET_FILE_DIR:IndComm>/resources/i18n/${lang}/LC_MESSAGES/"
        )
    endforeach()
    add_custom_target(i18n ALL DEPENDS ${I18N_MO_FILES})
    add_dependencies(IndComm i18n)
else()
    message(WARNING "msgfmt not found: falling back to pre-built .mo files in resources/i18n")
    list(APPEND I18N_COPY_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:IndComm>/resources/i18n"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${I18N_SRC_DIR}" "$<TARGET_FILE_DIR:IndComm>/resources/i18n"
    )
endif()

set(WX_DLLS
    "${WX_LIB_DIR}/wxbase335u_gcc_custom.dll"
    "${WX_LIB_DIR}/wxbase335u_net_gcc_custom.dll"
    "${WX_LIB_DIR}/wxmsw335u_core_gcc_custom.dll"
    "${WX_LIB_DIR}/wxmsw335u_aui_gcc_custom.dll"
)

add_custom_command(TARGET IndComm POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${WX_DLLS} "$<TARGET_FILE_DIR:IndComm>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:modbus> "$<TARGET_FILE_DIR:IndComm>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:sqlite.library> "$<TARGET_FILE_DIR:IndComm>"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:IndComm>/resources/icons"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons" "$<TARGET_FILE_DIR:IndComm>/resources/icons"
    ${I18N_COPY_COMMANDS}
    COMMENT "Copying DLLs, icons and i18n to GUI output"
)

add_custom_command(TARGET indcomm-cli POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${WX_DLLS} "$<TARGET_FILE_DIR:indcomm-cli>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:modbus> "$<TARGET_FILE_DIR:indcomm-cli>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:sqlite.library> "$<TARGET_FILE_DIR:indcomm-cli>"
    COMMENT "Copying DLLs to CLI output"
)

if(ICU_BUILD_TESTS)
    enable_testing()
    include(CTest)
    add_subdirectory(tests)
endif()

# =========================================================================
# 阶段 14：安装规则与 CPack 打包
# =========================================================================
if(WIN32)
    install(TARGETS IndComm RUNTIME DESTINATION .)
    install(TARGETS indcomm-cli RUNTIME DESTINATION .)

    if(ICU_HAS_MODBUSLIB)
        install(FILES $<TARGET_FILE:modbus> DESTINATION .)
    endif()

    install(FILES $<TARGET_FILE:sqlite.library> DESTINATION .)
    install(FILES ${WX_DLLS} DESTINATION .)

    install(FILES "${APP_ICO}" DESTINATION .)

    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/"
            DESTINATION "resources/icons"
            FILES_MATCHING PATTERN "*")

    if(GETTEXT_MSGFMT_EXECUTABLE)
        foreach(lang ${I18N_LANGUAGES})
            install(FILES "${I18N_BUILD_DIR}/${lang}/LC_MESSAGES/IndustrialCommunicationUtility.mo"
                    DESTINATION "resources/i18n/${lang}/LC_MESSAGES")
        endforeach()
    else()
        install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/resources/i18n/"
                DESTINATION "resources/i18n"
                FILES_MATCHING PATTERN "*.mo")
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE" DESTINATION .)
    endif()
endif()

set(CPACK_PACKAGE_NAME "IndComm")
set(CPACK_PACKAGE_VENDOR "lee-yilong")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Industrial multi-protocol communication test utility")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "IndComm-${PROJECT_VERSION}-win64")
set(CPACK_GENERATOR "ZIP")

if(WIN32)
    list(APPEND CPACK_GENERATOR "NSIS")

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
        set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
    endif()

    set(CPACK_NSIS_INSTALL_ROOT "$LOCALAPPDATA\\\\Programs")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "IndComm")

    set(CPACK_NSIS_DISPLAY_NAME "IndComm")
    set(CPACK_NSIS_PACKAGE_NAME "IndComm")
    set(CPACK_NSIS_CONTACT "https://github.com/lee-yilong/IndCommUtil")
    set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/lee-yilong/IndCommUtil")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "IndComm.exe")
    set(CPACK_NSIS_EXECUTABLES_DIRECTORY ".")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MODIFY_PATH OFF)
    set(CPACK_NSIS_MENU_LINKS "IndComm.exe" "IndComm")

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/app.ico")
        set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/app.ico")
        set(CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/app.ico")
    endif()

    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortCut '$DESKTOP\\\\IndComm.lnk' '$INSTDIR\\\\IndComm.exe' '$INSTDIR\\\\app.ico'"
        "CreateShortCut '$SMPROGRAMS\\\\IndComm.lnk' '$INSTDIR\\\\IndComm.exe' '$INSTDIR\\\\app.ico'"
    )
    set(CPACK_NSIS_DELETE_ICONS_EXTRA
        "Delete '$DESKTOP\\\\IndComm.lnk'"
        "Delete '$SMPROGRAMS\\\\IndComm.lnk'"
    )
endif()

include(CPack)
