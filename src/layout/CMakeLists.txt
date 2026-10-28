# The widgets that show and edit layout trees.
mirante_add_library(mirante_layout LIBS mirante_items Qt6::Widgets)

# Icons of the app's own buttons, such as the lock and pin buttons, served as
# :/icons/<name>.svg. The ui library links this one, so it uses them too.
file(GLOB app_icons CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/resources/icons/*.svg")
qt_add_resources(mirante_layout app_icons
    PREFIX /icons
    BASE "${PROJECT_SOURCE_DIR}/resources/icons"
    FILES ${app_icons})
