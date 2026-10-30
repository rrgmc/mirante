# M10 code plan: packaging

This plan builds [milestone M10](../roadmap.md#m10-packaging). After M10, pushing a tag `vX.Y.Z` publishes a GitHub release with a Windows setup, a notarised macOS disk image and an Ubuntu `.deb` package, and CI builds and tests on macOS again.

## Decisions

All accepted:

- [Decision 0098](../decisions/0098-packages-from-cmake-install.md): `cmake --install` makes a self-contained app with Qt's deploy script, CPack makes the package, and `MIRANTE_VERSION` carries the tag's version.
- [Decision 0099](../decisions/0099-linux-package-in-opt.md): the `.deb` installs into `/opt/mirante` with its own Qt, a desktop file and an AppArmor profile for Chromium's sandbox.
- [Decision 0101](../decisions/0101-macos-release-signing.md): the bundle identifier is `com.rangelreale.mirante`; the release is universal, signed with a Developer ID, with entitlements for `QtWebEngineProcess`, and notarised.

[Decision 0077](../decisions/0077-no-macos-in-ci-during-development.md) already says that M10 turns macOS in CI back on.

Smaller choices made here:

- **Package names.** `mirante-<version>-win64.exe`, `mirante-<version>-macos.dmg` and `mirante_<version>_amd64.deb`, the last in Debian's own form.
- **Windows setup.** NSIS, installing for all users into `Program Files\Mirante`, with a Start menu shortcut and an uninstaller. A new version uninstalls the old one first. It is not signed, so SmartScreen warns on the first start. Starting at sign-in is not offered ([decision 0075](../decisions/0075-hide-to-tray-and-show-hide-hotkey.md) left it out).
- **Translations.** Qt's deploy script runs with `NO_TRANSLATIONS`, and the install copies `qtbase_pt_BR.qm` itself into the folder that `QLibraryInfo::TranslationsPath` gives in each layout. Qt WebEngine's own locale files are part of its resources and are copied by the deploy step.
- **CI packages.** Each CI build on Windows, macOS and Linux also makes its package and keeps it as a build artifact for 7 days. The packages from CI are unsigned (ad hoc on macOS).
- **Checking a package.** `tools/check-package.py <install folder>` fails if the installed app lacks the executable, `qtbase_pt_BR.qm` where Qt looks for it, the WebP plugin, or `QtWebEngineProcess` and its resources. CI and the release workflow run it on the install folder before CPack.

## Changes to the architecture

None in the code. [Architecture](../architecture.md#build) describes the install and the packages in its build section, and drops "Linux builds are not packaged in the first version".

## Part 1: version and identity

- `CMakeLists.txt`: a cache variable `MIRANTE_VERSION`, default `0.1.0`, checked against `^[0-9]+\.[0-9]+\.[0-9]+$`, passed to `project(VERSION)`.
- `src/CMakeLists.txt`: `MACOSX_BUNDLE_GUI_IDENTIFIER com.rangelreale.mirante`, `MACOSX_BUNDLE_BUNDLE_VERSION` and `MACOSX_BUNDLE_SHORT_VERSION_STRING` from the version.
- `resources/app/mirante.rc` becomes `mirante.rc.in`, configured with a `VERSIONINFO` block, so the file's properties and the setup show the version.

## Part 2: the install

New `cmake/MiranteInstall.cmake`, included from `src/CMakeLists.txt` after the executable, with `mirante_add_install(<target>)`:

- `install(TARGETS)` with `BUNDLE DESTINATION .` and `RUNTIME DESTINATION bin`, `LICENSE` beside it.
- `qt_generate_deploy_app_script(... NO_TRANSLATIONS NO_UNSUPPORTED_PLATFORM_ERROR)`, with `--no-compiler-runtime --no-system-d3d-compiler --no-system-dxc-compiler --no-opengl-sw` on Windows, and `install(SCRIPT)`.
- `qtbase_pt_BR.qm` from the Qt in use, installed where the deployed app looks for translations. Which folder that is, per platform, is checked first by starting the installed app and logging `QLibraryInfo::path(QLibraryInfo::TranslationsPath)`.
- Windows: `InstallRequiredSystemLibraries` into `bin`.
- Linux, per [decision 0099](../decisions/0099-linux-package-in-opt.md): install prefix `/opt/mirante` for CPack; `resources/linux/mirante.desktop`; the icon from `resources/icons/mirante.svg`; the link `/usr/bin/mirante`; `resources/linux/apparmor-mirante`; `postinst` and `prerm` scripts in `resources/linux/`. If CPack's DEB generator does not take the absolute destinations under `/usr` and `/etc`, the package uses `CPACK_SET_DESTDIR`.
- macOS: the signing step of Part 4.

## Part 3: CPack

In `cmake/MiranteInstall.cmake`, after the install rules, `include(CPack)` with:

- Common: name, vendor, summary, homepage, licence file, the version, and `CPACK_PACKAGE_EXECUTABLES mirante Mirante`.
- Windows: `NSIS`, the icon from `resources/app/mirante.ico`, `CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL`, the installed icon `bin\mirante.exe`.
- macOS: `DragNDrop`, volume name Mirante.
- Linux: `DEB`, `CPACK_DEBIAN_FILE_NAME DEB-DEFAULT`, `CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON` with `CPACK_DEBIAN_PACKAGE_SHLIBDEPS_PRIVATE_DIRS` pointing at the bundled `lib`, section `utils`, the maintainer from `Version.h`'s author, and the control scripts.
- `tools/check-package.py`, which takes an install folder and the platform's layout.

## Part 4: signing on macOS

- New `resources/macos/webengine.entitlements` with the three entitlements of [decision 0101](../decisions/0101-macos-release-signing.md).
- New `tools/sign-macos.sh <bundle> <identity>`. With `-` it signs the bundle ad hoc with `--deep`. With a certificate, it signs from the inside out with `--options runtime --timestamp`: every framework and library, then `QtWebEngineProcess.app` with the entitlements, then `QtWebEngineCore.framework`, then the app. It ends with `codesign --verify --deep --strict`.
- The install calls it after the deploy script, with `MIRANTE_MACOS_DISTRIBUTION_IDENTITY` or `-`.
- New `tools/release-macos.sh <version> [--upload]`, adapted from favdir's: checks the certificate and the notary credentials, configures a separate build folder `build/macos-release` with the `ci` preset, the version, the identity and `CMAKE_OSX_ARCHITECTURES=arm64;x86_64`, builds, tests, runs CPack, signs the disk image, notarises it, staples it and checks it with `spctl`. Credentials come from a stored notarytool profile or, in the workflow, from `MIRANTE_NOTARY_KEY`, `MIRANTE_NOTARY_KEY_ID` and `MIRANTE_NOTARY_ISSUER`.

## Part 5: CI

`.github/workflows/ci.yml`:

- `macos-14` back in the build matrix.
- After the tests, each build job runs `cmake --install` into a staging folder, `tools/check-package.py`, then `cpack`, and uploads the package as an artifact with `retention-days: 7`.
- On Windows, the job installs NSIS if the runner lacks it. On Linux, `dpkg-dev` for `dpkg-shlibdeps`, if missing.
- Tags do not start `ci.yml`: `push` gets `branches: ['**']`, since the release workflow builds tags itself.

## Part 6: the release workflow

New `.github/workflows/release.yml`, following favdir's:

- Triggers: a pushed tag `v[0-9]*.[0-9]*.[0-9]*`, or `workflow_dispatch` with a `tag` input. The code comes from the tag.
- Job `release`: checks the tag, gives the version, and creates the GitHub release with generated notes unless it exists.
- Job `windows-setup`: `tools/build.ps1 -Preset ci` with the version, check, `cpack`, `gh release upload --clobber`.
- Job `linux-deb`: the same steps as CI's Linux build, with the version, then the upload.
- Job `macos-dmg`: in environment `release`, actions pinned to commits, `persist-credentials: false`; imports the certificate into a temporary keychain, writes and checks the notary key, runs `tools/release-macos.sh`, uploads, and removes the keychain and key in an `always()` step. Secrets: `MACOS_CERTIFICATE_P12`, `MACOS_CERTIFICATE_PASSWORD`, `NOTARY_API_KEY`, `NOTARY_API_KEY_ID`, `NOTARY_API_ISSUER`.
- `tools/build.ps1` gains a `-Version` parameter that passes `-DMIRANTE_VERSION`.

## Part 7: documents

- Root `README.md`: an "Installing" section before "Building", with each package, the first-start warnings (SmartScreen on Windows; none on macOS for a release; the AppArmor profile on Linux), and a short first run: create or open a workspace, the sample data, adding a group, project and task, and where tokens are entered.
- New `docs/releasing.md`: how to release, the `release` environment with its tag rule and secrets, rerunning for an existing tag, `tools/release-macos.sh` by hand, and raising the version in `CMakeLists.txt` afterwards. Listed in `docs/README.md`.
- `docs/architecture.md`: the build section, as above.
- `docs/decisions/README.md`: the three records; 0077's status notes that M10 turned macOS back on.
- `docs/roadmap.md`: the boxes, as they are done, and a link to this plan.

## Order of work

The parts change different files, but each depends on the one before it, so one session does them in order, with one commit per part:

1. Part 1, then Part 2 and Part 3, checked on Windows: `cmake --install`, `tools/check-package.py`, `cpack`, and the installed app started with `-Settings` on a scratch file.
2. Part 4, which can be checked only on a Mac. CI checks the ad hoc path; the Developer ID path is checked by the release workflow or by the maintainer.
3. Part 5. Work branches are never pushed, so CI checks it when the maintainer pushes `master`.
4. Part 6 and Part 7.

## Checking

- Windows, by hand: run the setup, start Mirante from the Start menu, switch the language to Portuguese and see a standard dialog's buttons translated, drop a WebP image, open a web page, uninstall.
- Linux, by the maintainer or in a virtual machine: install the `.deb`, start it from the menu, open a web page with the sandbox on, remove the package.
- macOS, by the maintainer: open the release disk image on a Mac that never ran Mirante; Gatekeeper opens it without a warning.
- The release workflow, by the maintainer: push a test tag such as `v0.1.0`, check the three files on the release, then delete the release and tag if they are not meant to stay.

## Out of scope

- Signing the Windows setup.
- AppImage, Flatpak, Snap, RPM, ARM Linux.
- Automatic updates.
- Starting at sign-in.
