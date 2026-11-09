# Building Mirante

This repository builds Mirante from its core and its modules, which the build fetches at the release tags that `mirante.json` pins. The first configure needs the network.

## What you need

- Qt 6.8, with the modules that [the core's building guide](https://github.com/rrgmc/mirante-core/blob/master/BUILDING.md) lists
- CMake 3.21 or later, and Ninja
- On Windows, MSVC 2022, 64-bit. Qt WebEngine does not support MinGW.

## Building and testing

```sh
cmake --preset dev -DCMAKE_PREFIX_PATH=<Qt folder>
cmake --build --preset dev
ctest --preset dev
```

`<Qt folder>` is the folder of Qt's compiler build, such as `C:\Qt\6.8.3\msvc2022_64`. The `ci` preset builds a release instead.

On Windows, the core's script does all of it from a Developer PowerShell or any other shell. It downloads Qt if it is missing and enters the Visual Studio environment:

```sh
<mirante-core>/tools/build.ps1 -Source <this folder> -Preset ci
```

Add `-Package` to make the setup, `build/ci/mirante-<version>-win64.exe`.

## Trying a build

To start a build without touching your own data, give it its own settings and folders:

```sh
build/dev/bin/mirante --settings try/settings.ini --web-data try/web-data --no-keychain
```

## Releases

1. A pull request raises `MIRANTE_VERSION` in `CMakeLists.txt`, and runs `python3 ../mirante-core/tools/release-changelog.py --root . release X.Y.Z`. That moves the entries under Unreleased in [CHANGELOG.md](CHANGELOG.md), and the fragments in [changelog.d](changelog.d/README.md), to a section for the new version.
2. After it merges, the tag `vX.Y.Z` is pushed on that commit. A tag `vX.Y.Z-rc.N` publishes a prerelease.
3. `.github/workflows/release.yml` builds and tests on Windows, macOS and Linux. A separate job signs and notarises the macOS disk image with the Developer ID, and the release is published.

Once a week, `.github/workflows/update.yml` opens a pull request that moves the pins to the newest release tags of the core and the modules, and raises the patch version. It needs the secret `MIRANTE_UPDATE_TOKEN`: a fine-grained token with **Contents**, **Pull requests** and **Workflows** set to **Read and write** on this repository. After merging one, its tag is pushed.

## Where the code is

The app's code, its tests and its design documents are in [`mirante-core`](https://github.com/rrgmc/mirante-core), and the modules in [`mirante-modules`](https://github.com/rrgmc/mirante-modules) and [`mirante-modules-work`](https://github.com/rrgmc/mirante-modules-work). A change is made there, released, and reaches this repository when its pin moves.
