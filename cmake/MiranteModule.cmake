# The module mechanism. See docs/modules.md#building.
#
# Each module folder calls mirante_add_module(). The top level then calls
# mirante_finish_modules() once, which checks the module list and generates the
# registration code. The module lists are kept in global properties:
#
#   MIRANTE_ALL_MODULES          every module folder, enabled or not
#   MIRANTE_ENABLED_MODULES      the enabled modules
#   MIRANTE_MODULE_DEPENDS_<id>  the DEPENDS of one module

# mirante_add_module(<id>
#     SOURCES   <files...>
#     TESTS     <files...>
#     RESOURCES <files...>
#     DEPENDS   <module ids...>
#     QT_LIBS   <Qt components...>
#     DEFAULT   ON|OFF)
#
# QT_LIBS links further Qt components, such as WebEngineWidgets for a module
# that builds its own web view from ItemContext::profile(). See
# docs/decisions/0037-core-makes-web-views.md.
function(mirante_add_module id)
    cmake_parse_arguments(ARG "" "DEFAULT" "SOURCES;TESTS;RESOURCES;DEPENDS;QT_LIBS" ${ARGN})
    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Module ${id}: unknown arguments: ${ARG_UNPARSED_ARGUMENTS}")
    endif()

    if(NOT id MATCHES "^[a-z0-9]+$")
        message(FATAL_ERROR "Module id \"${id}\" must have lowercase letters and digits only.")
    endif()
    if(id STREQUAL "dash")
        message(FATAL_ERROR "Module id \"dash\" is reserved for the core.")
    endif()
    get_property(all_modules GLOBAL PROPERTY MIRANTE_ALL_MODULES)
    if(id IN_LIST all_modules)
        message(FATAL_ERROR "Module ${id} is added twice.")
    endif()
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "Module ${id} has no SOURCES.")
    endif()
    if(NOT DEFINED ARG_DEFAULT)
        set(ARG_DEFAULT ON)
    endif()

    set_property(GLOBAL APPEND PROPERTY MIRANTE_ALL_MODULES ${id})
    set_property(GLOBAL PROPERTY MIRANTE_MODULE_DEPENDS_${id} ${ARG_DEPENDS})

    # MIRANTE_MODULES, when set, is the exact list of enabled modules.
    string(TOUPPER "${id}" upper_id)
    set(option_name MIRANTE_MODULE_${upper_id})
    set(option_help "Build the ${id} module")
    if(DEFINED MIRANTE_MODULES)
        if(id IN_LIST MIRANTE_MODULES)
            set(${option_name} ON CACHE BOOL "${option_help}" FORCE)
        else()
            set(${option_name} OFF CACHE BOOL "${option_help}" FORCE)
        endif()
    else()
        option(${option_name} "${option_help}" ${ARG_DEFAULT})
    endif()

    if(NOT ${option_name})
        message(STATUS "Module ${id}: off")
        return()
    endif()
    message(STATUS "Module ${id}: on")
    set_property(GLOBAL APPEND PROPERTY MIRANTE_ENABLED_MODULES ${id})

    # Only the module interface and Qt. src/ is never on the include path, so a
    # module cannot include core headers.
    set(target mirante_module_${id})
    add_library(${target} STATIC ${ARG_SOURCES})
    target_link_libraries(${target} PUBLIC mirante::module_api Qt6::Core Qt6::Gui Qt6::Widgets)
    foreach(qt_lib IN LISTS ARG_QT_LIBS)
        target_link_libraries(${target} PUBLIC Qt6::${qt_lib})
    endforeach()
    mirante_set_warnings(${target})

    # Qt 6 links the resources of a static library as object files, so no
    # Q_INIT_RESOURCE call is needed.
    if(ARG_RESOURCES)
        qt_add_resources(${target} ${target}_resources
            PREFIX "/modules/${id}"
            FILES ${ARG_RESOURCES})
    endif()

    foreach(test_source IN LISTS ARG_TESTS)
        get_filename_component(test_name "${test_source}" NAME_WE)
        mirante_add_test(module_${id}_${test_name}
            SOURCES "${test_source}"
            LIBS ${target} mirante_items mirante::module_api_testing)
    endforeach()
endfunction()

# mirante_finish_modules(<executable>)
#
# Called once, after every module folder is added. Checks MIRANTE_MODULES and
# DEPENDS, generates enabled_modules.cpp from enabled_modules.cpp.in, and adds it
# and the module libraries to <executable>.
function(mirante_finish_modules target)
    get_property(all_modules GLOBAL PROPERTY MIRANTE_ALL_MODULES)
    get_property(enabled GLOBAL PROPERTY MIRANTE_ENABLED_MODULES)

    if(DEFINED MIRANTE_MODULES)
        foreach(id IN LISTS MIRANTE_MODULES)
            if(NOT id IN_LIST all_modules)
                message(FATAL_ERROR "MIRANTE_MODULES names the module \"${id}\", which does not exist. "
                    "The modules are: ${all_modules}")
            endif()
        endforeach()
    endif()

    foreach(id IN LISTS enabled)
        get_property(depends GLOBAL PROPERTY MIRANTE_MODULE_DEPENDS_${id})
        foreach(dependency IN LISTS depends)
            if(NOT dependency IN_LIST all_modules)
                message(FATAL_ERROR "Module ${id} depends on the module \"${dependency}\", which does not exist.")
            endif()
            if(NOT dependency IN_LIST enabled)
                message(FATAL_ERROR "Module ${id} depends on the module ${dependency}, which is disabled. "
                    "Enable ${dependency} or disable ${id}.")
            endif()
        endforeach()
    endforeach()

    list(SORT enabled)
    set(MIRANTE_MODULE_DECLARATIONS "")
    set(MIRANTE_MODULE_CALLS "")
    set(ids "")
    foreach(id IN LISTS enabled)
        string(APPEND MIRANTE_MODULE_DECLARATIONS
            "namespace mirante::modules::${id} {\n"
            "void registerModule(mirante::ModuleRegistrar &registrar);\n"
            "}\n")
        string(APPEND MIRANTE_MODULE_CALLS
            "    registry.registerModule(QStringLiteral(\"${id}\"), &mirante::modules::${id}::registerModule);\n")
        list(APPEND ids "QStringLiteral(\"${id}\")")
        target_link_libraries(${target} PRIVATE mirante_module_${id})
    endforeach()
    list(JOIN ids ", " MIRANTE_MODULE_IDS)

    set(generated "${PROJECT_BINARY_DIR}/generated/enabled_modules.cpp")
    configure_file("${PROJECT_SOURCE_DIR}/cmake/enabled_modules.cpp.in" "${generated}" @ONLY)
    target_sources(${target} PRIVATE "${generated}")
endfunction()
