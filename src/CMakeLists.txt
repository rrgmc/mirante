# One static library per folder. Dependencies point downwards only:
# ui -> layout -> items -> core, with platform used by ui only.
# See docs/architecture.md#source-folders.

add_subdirectory(module_api)
add_subdirectory(core)
add_subdirectory(items)
add_subdirectory(layout)
add_subdirectory(platform)
add_subdirectory(ui)
add_subdirectory(modules)

qt_add_executable(mirante WIN32 MACOSX_BUNDLE main.cpp)
# The executable's icon on Windows. The window icon comes from resources/icons.
if(WIN32)
    target_sources(mirante PRIVATE "${PROJECT_SOURCE_DIR}/resources/app/mirante.rc")
endif()
target_link_libraries(mirante PRIVATE mirante_ui)
mirante_set_warnings(mirante)
mirante_finish_modules(mirante)

# The fixture data folder, loaded unless the app is started with --data.
file(GLOB_RECURSE fixture_files CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/resources/fixtures/*.json")
if(fixture_files)
    qt_add_resources(mirante fixtures
        PREFIX /fixtures
        BASE "${PROJECT_SOURCE_DIR}/resources/fixtures"
        FILES ${fixture_files})
endif()
