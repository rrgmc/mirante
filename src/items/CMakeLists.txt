# Item type registry, the core side of the module interface, and placements.
# Platform for starting terminals and programs; see decision 0082.
mirante_add_library(mirante_items LIBS mirante_core mirante_web mirante_api mirante_platform mirante::module_api Qt6::Widgets)
