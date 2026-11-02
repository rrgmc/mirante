# A small project that uses cmake/MiranteModule.cmake, to test DEPENDS. See
# run_test.cmake. The module interface is a stub, so nothing of the core is built.
cmake_minimum_required(VERSION 3.21)
project(MiranteDependsTest LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets)

set(MIRANTE_PCH OFF)
get_filename_component(repo "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
list(APPEND CMAKE_MODULE_PATH "${repo}/cmake")
include(MiranteTargets)
include(MiranteModule)

add_library(mirante_module_api INTERFACE)
add_library(mirante::module_api ALIAS mirante_module_api)
target_link_libraries(mirante_module_api INTERFACE Qt6::Core)

# beta uses alpha's header and code. gamma uses them without listing alpha, so
# it must not compile.
add_subdirectory(beta)
add_subdirectory(alpha)
if(WITH_GAMMA)
    add_subdirectory(gamma)
endif()

add_executable(depends_app main.cpp)
mirante_finish_modules(depends_app)
target_include_directories(depends_app PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
