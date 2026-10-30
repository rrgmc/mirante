# One static library per folder. Dependencies point downwards only:
# ui -> layout -> items -> web, api -> core, with platform used by ui only.
# See docs/architecture.md#source-folders.

add_subdirectory(module_api)
add_subdirectory(core)
add_subdirectory(web)
add_subdirectory(api)
add_subdirectory(items)
add_subdirectory(layout)
add_subdirectory(platform)
add_subdirectory(ui)
add_subdirectory(modules)

qt_add_executable(mirante WIN32 MACOSX_BUNDLE main.cpp)
# The executable's icon and version on Windows, and the bundle's icon,
# identifier and version on macOS. The Finder and keychain prompts show the
# icon, and the keychain knows the app by its identifier (decision 0101).
# While the app runs, the window icon comes from resources/icons.
# tools/make-macos-icon.sh makes the .icns from it.
if(WIN32)
    configure_file("${PROJECT_SOURCE_DIR}/resources/app/mirante.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/generated/mirante.rc" @ONLY)
    target_sources(mirante PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated/mirante.rc")
elseif(APPLE)
    set(bundle_icon "${PROJECT_SOURCE_DIR}/resources/app/mirante.icns")
    set_source_files_properties("${bundle_icon}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(mirante PRIVATE "${bundle_icon}")
    set_target_properties(mirante PROPERTIES
        MACOSX_BUNDLE_ICON_FILE mirante.icns
        MACOSX_BUNDLE_GUI_IDENTIFIER com.rangelreale.mirante
        MACOSX_BUNDLE_BUNDLE_NAME Mirante
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_COPYRIGHT "Copyright (c) 2026 Rangel Reale")
endif()
target_link_libraries(mirante PRIVATE mirante_ui)
mirante_set_warnings(mirante)
mirante_finish_modules(mirante)
# A copy of the app with its Qt files, in <build>/bin.
mirante_add_bin_folder(mirante)
# The install and the package.
mirante_add_install(mirante)

# The translations, compiled into the app's resources under :/i18n. lupdate
# reads every source file of the app and of all modules, also those left out
# of this build, so the translation files stay complete in every preset. Test
# sources are not read. The files keep no source locations, so a change of
# code moves no lines in them. update_translations runs lupdate. See
# translations/README.md and decision 0086.
file(GLOB_RECURSE translation_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.mm")
list(FILTER translation_sources EXCLUDE REGEX "/tests/|/module_api/testing/")
list(REMOVE_ITEM translation_sources "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
qt_add_translations(mirante
    SOURCE_TARGETS mirante
    SOURCES ${translation_sources}
    # lupdate follows the includes to find the class of each tr().
    INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/module_api"
    TS_FILE_DIR "${PROJECT_SOURCE_DIR}/translations"
    RESOURCE_PREFIX /i18n
    LUPDATE_OPTIONS -locations none -no-obsolete)

# The fixture data folder, loaded unless the app is started with --data.
file(GLOB_RECURSE fixture_files CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/resources/fixtures/*.json"
    "${PROJECT_SOURCE_DIR}/resources/fixtures/*.mirante")
if(fixture_files)
    qt_add_resources(mirante fixtures
        PREFIX /fixtures
        BASE "${PROJECT_SOURCE_DIR}/resources/fixtures"
        FILES ${fixture_files})
endif()
