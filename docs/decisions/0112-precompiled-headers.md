# 0112. Precompiled headers, with ccache

- Status: Accepted
- Date: 2026-10-31

## Context

After [decision 0110](0110-ci-runs-once-per-change-and-caches-compiling.md), a build that ccache cannot serve still compiled everything from the start. A clean `ci` build on Windows took 1950 seconds of compiling. Most of it went to parsing the same Qt headers again in each file: about 690 seconds for the 134 test files, 585 seconds for the libraries. Each test also compiled its own copy of `tests/support/OffscreenByDefault.cpp`, which took 310 seconds in all.

## Decision

- **`cmake/MirantePrecompiled.h` is precompiled.** It holds the standard and Qt headers that most files include. A target gets only the Qt headers of the Qt modules it links, chosen by Qt's `QT_*_LIB` macros.
- **Each library makes its own precompiled header.** The tests of one folder, or of one module, link the same libraries, so they share the precompiled header of their first test.
- **`OffscreenByDefault.cpp` is compiled once**, in the object library `mirante_test_support`, which every test links.
- **ccache stays.** CMake passes ccache the sloppiness it needs to cache files compiled with a precompiled header. Without it, ccache does not cache them.
- **`MIRANTE_PCH=OFF` turns precompiled headers off.** Objective-C++ files never use them.

## Alternatives

- **Precompiled headers instead of ccache.** Precompiled headers make each compile cheaper, but ccache skips the files that did not change. With both, a local build where ccache has every file takes 30 seconds, against 51 seconds with precompiled headers alone.
- **One precompiled header for all tests.** Tests of different folders link different Qt modules, so their compile definitions differ, and a precompiled header can be shared only when they are the same.
- **A precompiled header for each test.** Each test has one file, so making the precompiled header would cost as much as it saves.

## Consequences

- A clean `ci` build on Windows takes 777 seconds of compiling instead of 1950. On CI, a build that ccache cannot serve took 5.9 minutes on Windows instead of 10.4.
- The precompiled header is included in every file, so a file that forgets an include still compiles. The `no-modules` CI job builds with `MIRANTE_PCH=OFF` and finds such files outside the modules.
- A change to `cmake/MirantePrecompiled.h` compiles everything again.
