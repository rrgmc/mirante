# Tokens, the modules' servers and the status cache. See
# docs/architecture.md#api and docs/integrations.md.
mirante_add_library(mirante_api LIBS mirante_core mirante::module_api Qt6::Network qt6keychain)
