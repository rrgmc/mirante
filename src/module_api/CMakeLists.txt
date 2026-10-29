# The only headers a module may include. See docs/modules.md#the-module-interface.
add_library(mirante_module_api INTERFACE)
add_library(mirante::module_api ALIAS mirante_module_api)
target_include_directories(mirante_module_api INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
# Widgets for ItemEditor.h.
target_link_libraries(mirante_module_api INTERFACE Qt6::Core Qt6::Gui Qt6::Widgets)
