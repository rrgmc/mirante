# The only headers a module may include. See docs/modules.md#the-module-interface.
add_library(mirante_module_api INTERFACE)
add_library(mirante::module_api ALIAS mirante_module_api)
target_include_directories(mirante_module_api INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
# Widgets for ItemEditor.h, Network for ApiClient.h.
target_link_libraries(mirante_module_api INTERFACE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network)

add_subdirectory(widgets)
target_link_libraries(mirante_module_api INTERFACE mirante_module_widgets)

# Fakes of the module interface for module tests: <mirante/testing/...>.
# mirante_add_module() links it to a module's tests only, so module code
# cannot include it.
add_library(mirante_module_api_testing INTERFACE)
add_library(mirante::module_api_testing ALIAS mirante_module_api_testing)
target_include_directories(mirante_module_api_testing INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/testing")
target_link_libraries(mirante_module_api_testing INTERFACE mirante::module_api)
