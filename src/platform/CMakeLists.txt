# Code that differs between Windows, macOS and Linux. It links qtkeychain and
# QHotkey, so every CI build proves that they build.
mirante_add_library(mirante_platform LIBS Qt6::Widgets qt6keychain QHotkey::QHotkey)

# macOS code in Objective-C++, which the library's *.cpp glob leaves out.
if(APPLE)
    target_sources(mirante_platform PRIVATE AttachedDialogsMac.mm TrayMac.mm)
    set_source_files_properties(TrayMac.mm PROPERTIES COMPILE_OPTIONS -fobjc-arc)
    target_link_libraries(mirante_platform PRIVATE "-framework AppKit")
endif()
