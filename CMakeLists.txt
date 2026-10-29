cmake_minimum_required(VERSION 3.21)

project(mirante
    VERSION 0.1
    DESCRIPTION "A personal all-work desktop dashboard"
    HOMEPAGE_URL "https://github.com/rrgmc/mirante"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets Network WebChannel WebEngineWidgets Test)
qt_standard_project_setup()

option(MIRANTE_WERROR "Treat compiler warnings as errors" OFF)

include(MiranteTargets)
include(Dependencies)
include(MiranteModule)

enable_testing()

configure_file(cmake/Version.h.in "${CMAKE_CURRENT_BINARY_DIR}/generated/Version.h" @ONLY)

add_subdirectory(src)
add_subdirectory(tests)
