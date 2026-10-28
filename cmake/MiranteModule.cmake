# The module mechanism. See docs/modules.md#building.
#
# This is the T0 stub: the signatures are final, but no module is built and the
# generated registration is empty. Task T2 implements the mechanism.

# mirante_add_module(<id>
#     SOURCES   <files...>
#     TESTS     <files...>
#     RESOURCES <files...>
#     DEPENDS   <module ids...>
#     DEFAULT   ON|OFF)
function(mirante_add_module id)
    cmake_parse_arguments(ARG "" "DEFAULT" "SOURCES;TESTS;RESOURCES;DEPENDS" ${ARGN})
    message(STATUS "Module ${id}: not built, the module mechanism is a stub until task T2")
endfunction()

# mirante_finish_modules(<executable>)
#
# Called once, after every module folder is added. Generates enabled_modules.cpp
# from enabled_modules.cpp.in and adds it to <executable>.
function(mirante_finish_modules target)
    set(MIRANTE_MODULE_DECLARATIONS "")
    set(MIRANTE_MODULE_CALLS "")
    set(MIRANTE_MODULE_IDS "")
    set(generated "${PROJECT_BINARY_DIR}/generated/enabled_modules.cpp")
    configure_file("${PROJECT_SOURCE_DIR}/cmake/enabled_modules.cpp.in" "${generated}" @ONLY)
    target_sources(${target} PRIVATE "${generated}")
endfunction()
