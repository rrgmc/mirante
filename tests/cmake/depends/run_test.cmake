# Tests DEPENDS of mirante_add_module(): with it, a module links the other module
# and reaches its headers; without it, the headers are out of reach; and an
# enabled module whose dependency is disabled stops configuration.
#
#   cmake -DSOURCE=<this folder> -DBINARY=<a folder> -DGENERATOR=<generator>
#         -DCOMPILER=<compiler> -DPREFIX=<Qt> -P run_test.cmake
#
# It builds a project of its own, so on Windows it needs the Visual Studio
# environment, which tools/build.ps1 sets up. Without it the test is skipped.

if(WIN32 AND NOT DEFINED ENV{VCToolsInstallDir})
    message(STATUS "SKIPPED: this needs the Visual Studio environment")
    return()
endif()

function(configure_project binary)
    file(REMOVE_RECURSE "${binary}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -S "${SOURCE}" -B "${binary}" -G "${GENERATOR}"
            "-DCMAKE_CXX_COMPILER=${COMPILER}" "-DCMAKE_PREFIX_PATH=${PREFIX}" -DCMAKE_BUILD_TYPE=Release ${ARGN}
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE output)
    set(result "${result}" PARENT_SCOPE)
    set(output "${output}" PARENT_SCOPE)
endfunction()

# With DEPENDS, everything builds and links.
configure_project("${BINARY}/good")
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Configuring failed:\n${output}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${BINARY}/good"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE output)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "A module with DEPENDS did not build:\n${output}")
endif()

# Without DEPENDS, the other module's header is out of reach.
configure_project("${BINARY}/undeclared" -DWITH_GAMMA=ON)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Configuring with gamma failed:\n${output}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${BINARY}/undeclared" --target mirante_module_gamma
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE output)
if(result EQUAL 0)
    message(FATAL_ERROR "A module without DEPENDS reached another module's header.")
endif()
if(NOT output MATCHES "Alpha\\.h")
    message(FATAL_ERROR "gamma failed for another reason than the missing header:\n${output}")
endif()

# A disabled dependency stops configuration, and names both modules.
configure_project("${BINARY}/disabled" "-DMIRANTE_MODULES=beta")
if(result EQUAL 0)
    message(FATAL_ERROR "A disabled dependency did not stop configuration.")
endif()
if(NOT output MATCHES "Module beta depends on the module alpha, which is disabled")
    message(FATAL_ERROR "A disabled dependency gave the wrong message:\n${output}")
endif()
