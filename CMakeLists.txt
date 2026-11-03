# Mirante, with the modules that mirante.json lists. The core does the rest. See
# docs/decisions/0125-how-modules-are-fetched.md in mirante-core.
cmake_minimum_required(VERSION 3.21)

# Raise it in the pull request that prepares a release, then push the tag vX.Y.Z.
set(MIRANTE_VERSION "0.8.2" CACHE STRING "Mirante's version, as X.Y.Z")
project(mirante LANGUAGES NONE)
enable_testing()

set(MIRANTE_MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/mirante.json")
file(READ "${MIRANTE_MANIFEST}" manifest)
string(JSON core_repo GET "${manifest}" core repo)
string(JSON core_ref GET "${manifest}" core ref)

include(FetchContent)
FetchContent_Declare(mirante_core GIT_REPOSITORY "${core_repo}" GIT_TAG "${core_ref}")
FetchContent_MakeAvailable(mirante_core)
