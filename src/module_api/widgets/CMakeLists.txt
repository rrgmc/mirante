# Small widgets that native views of modules share, since modules cannot depend
# on each other. It uses only Qt and <mirante/...>, never core headers. See
# docs/modules.md#the-view-kit.
file(GLOB sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../mirante/widgets/*.h")
add_library(mirante_module_widgets STATIC ${sources})
target_include_directories(mirante_module_widgets PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(mirante_module_widgets PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets)
mirante_set_warnings(mirante_module_widgets)
mirante_autogen_tracks_sources(mirante_module_widgets)
