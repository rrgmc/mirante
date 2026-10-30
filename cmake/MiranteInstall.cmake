# The install and the packages. See docs/decisions/0098-packages-from-cmake-install.md.

# mirante_add_install(<target>)
#
# `cmake --install <build> --component Mirante` makes a self-contained app:
# Qt's deploy script copies the Qt libraries, plugins and Qt WebEngine next to
# <target>. `cpack` in the build folder then makes the package: an NSIS setup
# on Windows, a disk image on macOS and a .deb package on Linux.
#
# Every rule here is in the component Mirante, and the package holds only that
# component. qtkeychain's own install rules, for its library and headers, stay
# out of it.
function(mirante_add_install target)
    if(WIN32 OR APPLE)
        set(app_dir .)
    else()
        # docs/decisions/0099-linux-package-in-opt.md: the app with its Qt in
        # /opt/mirante, and the files the desktop needs under /usr and /etc.
        set(app_dir opt/mirante)
    endif()

    install(TARGETS ${target}
        BUNDLE DESTINATION . COMPONENT Mirante
        RUNTIME DESTINATION "${app_dir}/bin" COMPONENT Mirante)
    install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" DESTINATION "${app_dir}" COMPONENT Mirante)

    # The Visual C++ runtime comes from InstallRequiredSystemLibraries below.
    # Qt Widgets on Direct3D 11 needs neither the shader compilers nor the
    # software OpenGL.
    set(deploy_options)
    if(WIN32)
        set(deploy_options --no-compiler-runtime --no-system-d3d-compiler
            --no-system-dxc-compiler --no-opengl-sw)
    endif()
    qt_generate_deploy_app_script(
        TARGET ${target}
        OUTPUT_SCRIPT deploy_script
        NO_TRANSLATIONS
        NO_UNSUPPORTED_PLATFORM_ERROR
        DEPLOY_TOOL_OPTIONS ${deploy_options})
    # Qt's deploy script copies into the install prefix. On Linux the app's
    # folder is below it, so the prefix points there while the script runs.
    if(NOT app_dir STREQUAL ".")
        install(CODE "
            set(mirante_install_prefix \"\${CMAKE_INSTALL_PREFIX}\")
            set(CMAKE_INSTALL_PREFIX \"\${CMAKE_INSTALL_PREFIX}/${app_dir}\")
        " COMPONENT Mirante)
    endif()
    install(SCRIPT "${deploy_script}" COMPONENT Mirante)
    if(NOT app_dir STREQUAL ".")
        install(CODE "set(CMAKE_INSTALL_PREFIX \"\${mirante_install_prefix}\")" COMPONENT Mirante)
    endif()

    # Qt's own texts in Portuguese, such as the buttons of standard dialogs.
    # main.cpp loads them from QLibraryInfo::TranslationsPath: translations/
    # under the app's prefix, or Contents/translations in the macOS bundle.
    # Mirante's own translations are in its resources.
    if(APPLE)
        set(translations_dir "${target}.app/Contents/translations")
    else()
        set(translations_dir "${app_dir}/translations")
    endif()
    foreach(language IN LISTS QT_I18N_TRANSLATED_LANGUAGES)
        install(FILES "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_TRANSLATIONS}/qtbase_${language}.qm"
            DESTINATION "${translations_dir}" COMPONENT Mirante)
    endforeach()

    if(WIN32)
        set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION bin)
        set(CMAKE_INSTALL_SYSTEM_RUNTIME_COMPONENT Mirante)
        include(InstallRequiredSystemLibraries)
    elseif(NOT APPLE)
        install(FILES "${PROJECT_SOURCE_DIR}/resources/linux/mirante.desktop"
            DESTINATION usr/share/applications COMPONENT Mirante)
        install(FILES "${PROJECT_SOURCE_DIR}/resources/icons/mirante.svg"
            DESTINATION usr/share/icons/hicolor/scalable/apps COMPONENT Mirante)
        install(FILES "${PROJECT_SOURCE_DIR}/resources/linux/apparmor-mirante"
            DESTINATION etc/apparmor.d RENAME mirante COMPONENT Mirante)
        install(CODE "
            file(MAKE_DIRECTORY \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/usr/bin\")
            file(CREATE_LINK ../../opt/mirante/bin/mirante
                \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/usr/bin/mirante\" SYMBOLIC)
        " COMPONENT Mirante)
    endif()

    # macdeployqt leaves the bundle's binaries unsigned, and macOS on Apple
    # silicon does not start unsigned code. This comes last, since a file added
    # after it would break the signature. tools/release-macos.sh sets the
    # Developer ID. See docs/decisions/0101-macos-release-signing.md.
    if(APPLE)
        set(MIRANTE_MACOS_DISTRIBUTION_IDENTITY "" CACHE STRING
            "macOS: the certificate that signs the installed app, with the hardened runtime. Empty signs ad hoc")
        set(identity "${MIRANTE_MACOS_DISTRIBUTION_IDENTITY}")
        if(NOT identity)
            set(identity -)
        endif()
        install(CODE "
            execute_process(
                COMMAND sh \"${PROJECT_SOURCE_DIR}/tools/sign-macos.sh\"
                    \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${target}.app\"
                    \"${identity}\"
                    \"${PROJECT_SOURCE_DIR}/resources/macos/webengine.entitlements\"
                COMMAND_ERROR_IS_FATAL ANY)
        " COMPONENT Mirante)
    endif()

    mirante_add_package()
endfunction()

# The package, made by `cpack` in the build folder.
function(mirante_add_package)
    set(CPACK_PACKAGE_NAME mirante)
    set(CPACK_PACKAGE_VENDOR "Rangel Reale")
    set(CPACK_PACKAGE_CONTACT "Rangel Reale <realerangel@gmail.com>")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
    set(CPACK_PACKAGE_HOMEPAGE_URL "${PROJECT_HOMEPAGE_URL}")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY Mirante)
    set(CPACK_PACKAGE_EXECUTABLES mirante Mirante)
    set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
    # The component Mirante only, in one package.
    set(CPACK_INSTALL_CMAKE_PROJECTS "${PROJECT_BINARY_DIR};${PROJECT_NAME};Mirante;/")

    if(WIN32)
        set(CPACK_GENERATOR NSIS)
        set(CPACK_PACKAGE_FILE_NAME "mirante-${PROJECT_VERSION}-win64")
        set(CPACK_NSIS_PACKAGE_NAME Mirante)
        set(CPACK_NSIS_DISPLAY_NAME Mirante)
        set(CPACK_NSIS_MUI_ICON "${PROJECT_SOURCE_DIR}/resources/app/mirante.ico")
        set(CPACK_NSIS_MUI_UNIICON "${PROJECT_SOURCE_DIR}/resources/app/mirante.ico")
        set(CPACK_NSIS_INSTALLED_ICON_NAME [=[bin\mirante.exe]=])
        set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
        set(CPACK_NSIS_URL_INFO_ABOUT "${PROJECT_HOMEPAGE_URL}")
        set(CPACK_NSIS_CONTACT "realerangel@gmail.com")
    elseif(APPLE)
        set(CPACK_GENERATOR DragNDrop)
        set(CPACK_PACKAGE_FILE_NAME "mirante-${PROJECT_VERSION}-macos")
        set(CPACK_DMG_VOLUME_NAME Mirante)
    else()
        set(CPACK_GENERATOR DEB)
        set(CPACK_PACKAGING_INSTALL_PREFIX /)
        # CPack stages the package in a folder of this name. The .deb itself
        # gets Debian's name, mirante_<version>_amd64.deb.
        set(CPACK_PACKAGE_FILE_NAME "mirante-${PROJECT_VERSION}-linux")
        set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
        set(CPACK_DEBIAN_PACKAGE_SECTION utils)
        set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
        # dpkg-shlibdeps finds the bundled Qt libraries here, and leaves them
        # out of the dependencies.
        set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS_PRIVATE_DIRS "${PROJECT_BINARY_DIR}/_CPack_Packages/Linux/DEB/${CPACK_PACKAGE_FILE_NAME}/opt/mirante/lib")
        set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
            "${PROJECT_SOURCE_DIR}/resources/linux/postinst"
            "${PROJECT_SOURCE_DIR}/resources/linux/prerm")
        set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION ON)
    endif()

    include(CPack)
endfunction()
