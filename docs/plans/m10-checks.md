# M10 checks by hand

These are the checks for [milestone M10](../roadmap.md#m10-packaging) that CI cannot make. See [the M10 plan](m10-packaging.md) and [releasing](../releasing.md).

Tick each box, or note what is wrong next to it.

**Result, 2026-10-31:** the user ran the checks with release v0.1.0 and accepted M10. Every box is ticked.

## Before the first release

- [x] The GitHub environment `release` exists, admits only tags matching `v*`, and holds the five secrets listed in [releasing](../releasing.md#the-release-environment).
- [x] After `master` is pushed, CI is green on Windows, macOS and Linux, and each build run has its package as an artifact.

## Windows setup

Made with `tools/build.ps1 -Preset release -Package`, or taken from CI or a release.

- [x] The setup shows the licence, installs into `Program Files\Mirante`, and adds Mirante to the Start menu.
- [x] The file's properties show the version.
- [x] Mirante starts from the Start menu on a PC without Qt or Visual Studio.
- [x] With the language set to Portuguese, a standard dialog's buttons are in Portuguese, such as Cancelar.
- [x] A dropped WebP image shows.
- [x] A web page opens, and claude.ai logs in.
- [x] Settings > Apps uninstalls it, and installing a newer setup over an older one leaves one entry.

## macOS disk image

From a release, on a Mac that never ran Mirante.

- [x] Opening the disk image and the app gives no Gatekeeper warning.
- [x] `codesign --verify --deep --strict --verbose=2 /Applications/mirante.app` and `spctl --assess --verbose /Applications/mirante.app` pass.
- [x] Web pages load, which shows that `QtWebEngineProcess` runs under the hardened runtime.
- [x] Portuguese buttons and WebP images work, as on Windows.
- [x] The keychain asks once for each token and remembers **Always Allow**.

## Ubuntu package

On Ubuntu 24.04, in a virtual machine or on a spare machine.

- [x] `sudo apt install ./mirante_<version>_amd64.deb` installs it and pulls in the libraries it needs.
- [x] Mirante is in the applications menu with its icon, and its window shows that icon in the dock.
- [x] Web pages load with Chromium's sandbox on: `QTWEBENGINE_DISABLE_SANDBOX` is not set, and starting `mirante` from a terminal prints no sandbox error.
- [x] Portuguese buttons and WebP images work.
- [x] `sudo apt remove mirante` removes it, with its AppArmor profile.

## The release workflow

- [x] Pushing a test tag, such as `v0.1.0`, publishes a release with the three files, named with the version.
- [x] Running the workflow by hand from the same tag replaces the files and keeps the release.
- [x] A test release that should not stay is deleted with its tag.
