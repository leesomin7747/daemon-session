cmake_minimum_required(VERSION 3.24)
project(DaemonSession VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

include(CTest)
option(DAEMON_BUILD_DESKTOP "Build desktop app and overlay probes" ON)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Sql Network WebSockets)
if(BUILD_TESTING)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
endif()
add_subdirectory(core)
add_subdirectory(server)

if(DAEMON_BUILD_DESKTOP)
find_package(Qt6 6.8 REQUIRED COMPONENTS Gui Widgets Quick QuickControls2)
if(BUILD_TESTING)
    find_package(Qt6 6.8 REQUIRED COMPONENTS QuickTest)
endif()
add_library(daemon_overlay STATIC
    desktop/src/OverlayWindow.cpp
    desktop/src/OverlayWindow.h
)
target_include_directories(daemon_overlay PUBLIC desktop/src)
target_link_libraries(daemon_overlay PUBLIC daemon_core Qt6::Core Qt6::Gui Qt6::Quick)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(LayerShellQt REQUIRED)
    target_link_libraries(daemon_overlay PRIVATE LayerShellQt::Interface)
    target_compile_definitions(daemon_overlay PRIVATE DAEMON_LAYER_SHELL)
endif()

add_subdirectory(probes/overlay)
add_subdirectory(desktop)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
