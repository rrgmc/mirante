# M10 checks by hand

These are the checks for [milestone M10](../roadmap.md#m10-packaging) that CI cannot make. See [the M10 plan](m10-packaging.md) and [releasing](../releasing.md).

Tick each box, or note what is wrong next to it.

## Before the first release

- [ ] The GitHub environment `release` exists, admits only tags matching `v*`, and holds the five secrets listed in [releasing](../releasing.md#the-release-environment).
- [ ] After `master` is pushed, CI is green on Windows, macOS and Linux, and each build run has its package as an artifact.

## Windows setup

Made with `tools/build.ps1 -Preset release -Package`, or taken from CI or a release.

- [ ] The setup shows the licence, installs into `Program Files\Mirante`, and adds Mirante to the Start menu.
- [ ] The file's properties show the version.
- [ ] Mirante starts from the Start menu on a PC without Qt or Visual Studio.
- [ ] With the language set to Portuguese, a standard dialog's buttons are in Portuguese, such as Cancelar.
- [ ] A dropped WebP image shows.
- [ ] A web page opens, and claude.ai logs in.
- [ ] Settings > Apps uninstalls it, and installing a newer setup over an older one leaves one entry.

## macOS disk image

From a release, on a Mac that never ran Mirante.

- [ ] Opening the disk image and the app gives no Gatekeeper warning.
- [ ] `codesign --verify --deep --strict --verbose=2 /Applications/mirante.app` and `spctl --assess --verbose /Applications/mirante.app` pass.
- [ ] Web pages load, which shows that `QtWebEngineProcess` runs under the hardened runtime.
- [ ] Portuguese buttons and WebP images work, as on Windows.
- [ ] The keychain asks once for each token and remembers **Always Allow**.

## Ubuntu package

On Ubuntu 24.04, in a virtual machine or on a spare machine.

- [ ] `sudo apt install ./mirante_<version>_amd64.deb` installs it and pulls in the libraries it needs.
- [ ] Mirante is in the applications menu with its icon, and its window shows that icon in the dock.
- [ ] Web pages load with Chromium's sandbox on: `QTWEBENGINE_DISABLE_SANDBOX` is not set, and starting `mirante` from a terminal prints no sandbox error.
- [ ] Portuguese buttons and WebP images work.
- [ ] `sudo apt remove mirante` removes it, with its AppArmor profile.

## The release workflow

- [ ] Pushing a test tag, such as `v0.1.0`, publishes a release with the three files, named with the version.
- [ ] Running the workflow by hand from the same tag replaces the files and keeps the release.
- [ ] A test release that should not stay is deleted with its tag.
