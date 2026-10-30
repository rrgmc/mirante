# 0098. Packages are made by `cmake --install` and CPack, and take their version from the tag

- Status: Accepted
- Date: 2026-10-30

## Context

M10 publishes Mirante as a Windows setup, a macOS disk image and an Ubuntu `.deb` package. Each must run on a machine without Qt. The build already makes a `bin` folder with the Qt files next to the app ([architecture](../architecture.md#build)), but only on Windows and macOS, and only for running a build on the machine that made it. Until now the version was fixed at 0.1 in `CMakeLists.txt`.

## Decision

- `cmake --install` makes a self-contained app on all three platforms. It runs Qt's own deploy script, from `qt_generate_deploy_app_script()`, which calls `windeployqt` on Windows, `macdeployqt` on macOS, and Qt's generic deployment on Linux, which also copies Qt WebEngine.
- CPack turns the installed app into the package: NSIS on Windows, DragNDrop on macOS, DEB on Linux.
- The `bin` folder stays as it is. It serves development, the install serves packages.
- The packages carry Qt's `qtbase` translation for Portuguese, which the app loads from `QLibraryInfo::TranslationsPath`, and Qt's image format plugins, among them WebP.
- The Windows setup also carries the Visual C++ runtime, through CMake's `InstallRequiredSystemLibraries`.
- The cache variable `MIRANTE_VERSION` sets the project version, as `X.Y.Z`. The release workflow passes the tag's version. Without it, the version is the one written in `CMakeLists.txt`.

## Alternatives

- **Package the `bin` folder.** It exists only on Windows and macOS, and it is made after every link, so adding the package's extra files there would slow each build.
- **Call `windeployqt` and `macdeployqt` from our own install code.** Qt's deploy script already does this, and it is the only one of the three that handles Qt WebEngine on Linux.
- **linuxdeploy, AppImage or Flatpak for Linux.** The roadmap asks for a `.deb` package. See [decision 0099](0099-linux-package-in-opt.md).
- **A version file in the repository.** It must then agree with the tag, which is one more thing to forget at a release.

## Consequences

- One `cpack` call in the build folder makes the package, the same way on CI, in the release workflow and on a developer's machine.
- An install copies all of Qt WebEngine, so making a package takes minutes and the packages are large.
- Builds outside the release workflow report the version in `CMakeLists.txt`, which must be raised by hand after a release.
