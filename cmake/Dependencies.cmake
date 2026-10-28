# Third-party libraries fetched at configure time, each pinned to a release.
# Both are built static and for Qt 6. mirante_platform links them, so every
# CI build proves that they build. They are first called in M5 and M8.

include(FetchContent)

# Third-party code does not get the project's warning flags, and its own
# tests are not built.
set(BUILD_SHARED_LIBS OFF)
set(BUILD_TESTING OFF)

# qtkeychain: tokens in the operating system keychain.
set(BUILD_WITH_QT6 ON CACHE BOOL "" FORCE)
set(BUILD_TRANSLATIONS OFF CACHE BOOL "" FORCE)
set(BUILD_TEST_APPLICATION OFF CACHE BOOL "" FORCE)
FetchContent_Declare(qtkeychain
    URL https://github.com/frankosterfeld/qtkeychain/archive/refs/tags/0.15.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP ON)

# QHotkey: the global hotkey.
set(QT_DEFAULT_MAJOR_VERSION 6 CACHE STRING "" FORCE)
set(QHOTKEY_EXAMPLES OFF CACHE BOOL "" FORCE)
set(QHOTKEY_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(qhotkey
    URL https://github.com/Skycoder42/QHotkey/archive/refs/tags/1.5.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP ON)

# QHotkey 1.5.0 asks for CMake 3.1, which CMake 4 refuses. This raises its
# policy level to 3.5 while the dependencies are configured.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
FetchContent_MakeAvailable(qtkeychain qhotkey)
unset(CMAKE_POLICY_VERSION_MINIMUM)
