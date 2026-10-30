# The bin folder: a copy of the app that runs on its own.

# mirante_add_bin_folder(<target>)
#
# After each link of <target>, copies it to <build>/bin and copies the Qt
# libraries, plugins and Qt WebEngine files it needs next to it. The copy in
# bin then starts without Qt's bin folder on PATH.
#
# - Windows: windeployqt. The Visual C++ runtime is not copied; Windows
#   machines usually have it installed.
# - macOS: the app bundle, made self-contained with macdeployqt, and signed
#   with MIRANTE_MACOS_SIGN_IDENTITY if that is set (decision 0075).
# - Linux: only the executable. It finds Qt through its RPATH, so it runs on
#   the machine that built it. Linux builds are not packaged yet.
function(mirante_add_bin_folder target)
    set(bin_dir "${PROJECT_BINARY_DIR}/bin")

    if(WIN32)
        if(NOT TARGET Qt6::windeployqt)
            message(WARNING "windeployqt was not found, so ${bin_dir} is not made.")
            return()
        endif()
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${bin_dir}"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:${target}>" "${bin_dir}"
            COMMAND Qt6::windeployqt --no-compiler-runtime --verbose 0
                "${bin_dir}/$<TARGET_FILE_NAME:${target}>"
            COMMENT "Deploying ${target} to ${bin_dir}"
            VERBATIM)
    elseif(APPLE)
        if(NOT TARGET Qt6::macdeployqt)
            message(WARNING "macdeployqt was not found, so ${bin_dir} is not made.")
            return()
        endif()
        # The bundle folder is two levels above the executable.
        set(bundle_dir "$<TARGET_FILE_DIR:${target}>/../..")
        set(bundle_name "$<TARGET_FILE_NAME:${target}>.app")
        set(sign_args)
        if(MIRANTE_MACOS_SIGN_IDENTITY)
            set(sign_args "-codesign=${MIRANTE_MACOS_SIGN_IDENTITY}")
        endif()
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E rm -rf "${bin_dir}/${bundle_name}"
            COMMAND "${CMAKE_COMMAND}" -E copy_directory
                "${bundle_dir}" "${bin_dir}/${bundle_name}"
            COMMAND Qt6::macdeployqt "${bin_dir}/${bundle_name}" -verbose=0 ${sign_args}
            COMMENT "Deploying ${target} to ${bin_dir}"
            VERBATIM)
    else()
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${bin_dir}"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:${target}>" "${bin_dir}"
            COMMENT "Copying ${target} to ${bin_dir}"
            VERBATIM)
    endif()
endfunction()
