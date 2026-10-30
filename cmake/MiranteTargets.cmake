# Helpers for Mirante's own libraries and tests.

# Applies the project's warning flags to a target.
function(mirante_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
        if(MIRANTE_WERROR)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(MIRANTE_WERROR)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

# mirante_add_library(<name> LIBS <targets...>)
#
# Builds a static library from every .cpp and .h file in the current folder.
# Files are found by globbing, so adding a file needs no change here.
# See docs/decisions/0017-parallel-tasks-with-fixed-interfaces.md.
function(mirante_add_library name)
    cmake_parse_arguments(ARG "" "" "LIBS" ${ARGN})
    file(GLOB sources CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
    add_library(${name} STATIC ${sources})
    target_include_directories(${name} PUBLIC
        "${PROJECT_SOURCE_DIR}/src"
        "${PROJECT_BINARY_DIR}/generated")
    target_link_libraries(${name} PUBLIC ${ARG_LIBS})
    mirante_set_warnings(${name})
endfunction()

# mirante_add_test(<name> SOURCES <files...> LIBS <targets...>)
#
# Adds one Qt Test executable as a CTest test. Tests run with the offscreen
# platform, so they need no display and open no windows, also when started by
# hand.
function(mirante_add_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBS" ${ARGN})
    add_executable(${name} ${ARG_SOURCES} "${PROJECT_SOURCE_DIR}/tests/support/OffscreenByDefault.cpp")
    target_link_libraries(${name} PRIVATE Qt6::Test ${ARG_LIBS})
    # Qt6::Test defines QT_TESTCASE_SOURCEDIR as the folder of the CMakeLists.txt,
    # so QFINDTESTDATA takes a path below that, such as "tests/data/x.json". A path
    # relative to the test's own file would rely on the path compiled into it,
    # which ccache can change.
    target_compile_definitions(${name} PRIVATE
        MIRANTE_FIXTURES_DIR="${PROJECT_SOURCE_DIR}/resources/fixtures")
    mirante_set_warnings(${name})
    # QSKIP and other Qt Test macros are variadic. Called with one argument,
    # they trip this pedantic Clang warning in C++17.
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(${name} PRIVATE -Wno-gnu-zero-variadic-macro-arguments)
    endif()
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

# mirante_add_tests(<folder> LIBS <targets...>)
#
# Adds every tst_*.cpp file in <folder> as its own test, named <folder>_<file>.
function(mirante_add_tests folder)
    cmake_parse_arguments(ARG "" "" "LIBS" ${ARGN})
    file(GLOB tests CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${folder}/tst_*.cpp")
    foreach(test_source IN LISTS tests)
        get_filename_component(test_name "${test_source}" NAME_WE)
        mirante_add_test("${folder}_${test_name}" SOURCES "${test_source}" LIBS ${ARG_LIBS})
    endforeach()
endfunction()
