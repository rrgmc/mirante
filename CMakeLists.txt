cmake_minimum_required(VERSION 3.21)

# Lets CMAKE_MSVC_DEBUG_INFORMATION_FORMAT below take effect. It must come
# before project().
if(POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
endif()

project(mirante
    VERSION 0.1
    DESCRIPTION "A personal all-work desktop dashboard"
    HOMEPAGE_URL "https://github.com/rrgmc/mirante"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Each worktree has its own build folder, since a CMake build folder belongs to
# one source folder. ccache, where it is installed, shares the compiling between
# them. It cannot cache MSVC's separate debug files, so objects hold their debug
# information.
find_program(MIRANTE_CCACHE ccache)
if(MIRANTE_CCACHE)
    message(STATUS "Compiling through ${MIRANTE_CCACHE}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${MIRANTE_CCACHE}")
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets Network WebChannel WebEngineWidgets Test)
qt_standard_project_setup()

# Qt 6.8's FindWrapOpenGL links the AGL framework, which Xcode 26 no longer
# ships. Nothing here uses AGL, so drop it.
if(APPLE AND TARGET WrapOpenGL::WrapOpenGL)
    get_target_property(_gl_libs WrapOpenGL::WrapOpenGL INTERFACE_LINK_LIBRARIES)
    list(FILTER _gl_libs EXCLUDE REGEX "AGL")
    set_target_properties(WrapOpenGL::WrapOpenGL PROPERTIES INTERFACE_LINK_LIBRARIES "${_gl_libs}")
endif()

option(MIRANTE_WERROR "Treat compiler warnings as errors" OFF)

include(MiranteTargets)
include(Dependencies)
include(MiranteModule)
include(MiranteDeploy)

enable_testing()

configure_file(cmake/Version.h.in "${CMAKE_CURRENT_BINARY_DIR}/generated/Version.h" @ONLY)

add_subdirectory(src)
add_subdirectory(tests)
