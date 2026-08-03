cmake_minimum_required(VERSION 3.21)

project(Waypane VERSION 0.1.2 LANGUAGES CXX)

configure_file(
    "${CMAKE_SOURCE_DIR}/src/core/version.h.in"
    "${CMAKE_BINARY_DIR}/generated/waypane/version.h"
    @ONLY
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

include(CTest)

find_package(ECM 6.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Network Widgets Test)
find_package(KF6 6.0 REQUIRED COMPONENTS CoreAddons KIO Parts Service Wallet)

qt_standard_project_setup(REQUIRES 6.5)
add_subdirectory(src)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

install(PROGRAMS tools/waypane-mcp DESTINATION bin)
