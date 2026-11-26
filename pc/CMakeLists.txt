# ScreenCTR — Windows application (Qt 6 + SDL3 + FFmpeg)
cmake_minimum_required(VERSION 3.21)
project(ScreenCTR VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

option(SCREENCTR_BUILD_TESTS "Build the Qt-free unit tests" ON)
get_filename_component(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/.. ABSOLUTE)

# ---- Qt-free core (shared with the tests) ----
add_library(screenctr_core STATIC
    ${ROOT}/protocol/src/sc_protocol.c
    core/Layout.cpp core/InputMapper.cpp core/StreamStats.cpp)
target_include_directories(screenctr_core PUBLIC ${ROOT}/protocol/include core)

if(SCREENCTR_BUILD_TESTS)
    enable_testing()
    add_executable(test_pc_core ${ROOT}/tests/test_pc_core.cpp)
    target_link_libraries(test_pc_core PRIVATE screenctr_core)
    add_test(NAME pc_core COMMAND test_pc_core)
endif()

# ---- Application ----
find_package(Qt6 6.5 COMPONENTS Widgets Network QUIET)
find_package(SDL3 CONFIG QUIET)
if(NOT Qt6_FOUND OR NOT SDL3_FOUND)
    message(WARNING "Qt 6 (Widgets, Network) and/or SDL3 not found: only the core library and tests are built.")
    return()
endif()

add_executable(ScreenCTR WIN32
    src/main.cpp
    src/Settings.cpp src/Settings.h
    src/StreamClient.cpp src/StreamClient.h
    src/ScreenView.cpp src/ScreenView.h
    src/GamepadManager.cpp src/GamepadManager.h
    src/InputSender.cpp src/InputSender.h
    src/KeyMap.cpp src/KeyMap.h
    src/Recorder.cpp src/Recorder.h
    src/SettingsDialog.cpp src/SettingsDialog.h
    src/MainWindow.cpp src/MainWindow.h
    resources/resources.qrc
    $<$<PLATFORM_ID:Windows>:resources/app.rc>)
target_link_libraries(ScreenCTR PRIVATE screenctr_core Qt6::Widgets Qt6::Network SDL3::SDL3)
target_compile_definitions(ScreenCTR PRIVATE SCREENCTR_VERSION="${PROJECT_VERSION}")
if(MSVC)
    target_compile_options(ScreenCTR PRIVATE /W4 /utf-8)
endif()

install(TARGETS ScreenCTR RUNTIME DESTINATION .)
if(WIN32)
    install(FILES $<TARGET_FILE:SDL3::SDL3> DESTINATION .)
    # windeployqt copies the Qt DLLs and plugins (platforms, styles, imageformats/qjpeg)
    find_program(WINDEPLOYQT windeployqt HINTS ${Qt6_DIR}/../../../bin)
    install(CODE "execute_process(COMMAND \"${WINDEPLOYQT}\" --release --no-translations --no-system-d3d-compiler \"\${CMAKE_INSTALL_PREFIX}/ScreenCTR.exe\")")
endif()

set(CPACK_PACKAGE_NAME "ScreenCTR")
set(CPACK_PACKAGE_VENDOR "Heivoli Network")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Two screens. One connection.")
set(CPACK_GENERATOR "ZIP")
include(CPack)
