# Mirante

A personal "all work" desktop dashboard for Windows, macOS and Linux. Mirante gathers the things you work on, such as merge requests, issues, pipelines, notes, folders and web pages, into dashboards of tasks and projects. It is written in Qt 6.

**Download** the latest release from [Releases](https://github.com/rrgmc/mirante/releases): a Windows setup, a signed and notarised macOS disk image, and an Ubuntu package.

## Where the code is

This repository builds the official app. It holds only a manifest, `mirante.json`, and the few files that build it. The code is in other repositories, which the build fetches at their release tags:

| Repository | Holds |
|---|---|
| [`mirante-core`](https://github.com/rrgmc/mirante-core) | The app, the module interface, the build and the design documents |
| [`mirante-modules`](https://github.com/rrgmc/mirante-modules) | Documents, folders, images, notes and web pages |
| [`mirante-modules-work`](https://github.com/rrgmc/mirante-modules-work) | GitHub, GitLab, Jira and Claude |

Anyone can build a Mirante of their own, with the modules they choose, from [`mirante-app-template`](https://github.com/rrgmc/mirante-app-template). The design is in [the core's documents](https://github.com/rrgmc/mirante-core/blob/master/docs/README.md), and [custom builds](https://github.com/rrgmc/mirante-core/blob/master/docs/plans/custom-builds.md) explains the split.

## Building

You need Qt 6.8 with the modules the core lists, CMake and Ninja, and on Windows MSVC 2022. See [building](https://github.com/rrgmc/mirante-core/blob/master/BUILDING.md) in the core.

```sh
cmake --preset dev -DCMAKE_PREFIX_PATH=<Qt folder>
cmake --build --preset dev
ctest --preset dev
```

On Windows, the core's `tools/build.ps1 -Source <this folder>` downloads Qt and does all of it.

## Releases

1. A pull request raises `MIRANTE_VERSION` in `CMakeLists.txt`.
2. After it merges, push the tag `vX.Y.Z` on that commit. A tag `vX.Y.Z-rc.N` publishes a prerelease.
3. `.github/workflows/release.yml` builds and tests on all three platforms. A separate job signs and notarises the macOS disk image with the Developer ID, and the release is published.

Once a week, `.github/workflows/update.yml` opens a pull request that moves the pins to the newest release tags of the core and the modules, and raises the patch version. It needs a fine-grained token, with **Contents**, **Pull requests** and **Workflows** set to **Read and write** on this repository, as the secret `MIRANTE_UPDATE_TOKEN`. After merging one, push its tag.

## License

Released under the [MIT License](LICENSE).
