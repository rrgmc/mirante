# 0101. The macOS release is a universal bundle, signed with a Developer ID and notarised

- Status: Accepted; bundle identifier superseded by [0103](0103-bundle-identifier-rrgmc.md)
- Date: 2026-10-30

## Context

macOS does not open a downloaded app unless it is signed with a Developer ID certificate and notarised by Apple. Notarisation requires the hardened runtime on every executable in the bundle. Qt WebEngine's helper, `QtWebEngineProcess`, runs Chromium's JavaScript engine, which needs memory that is both writable and executable, and the hardened runtime forbids that unless the helper has entitlements that allow it. The bundle identifier was not set, so Qt made up one from `com.yourcompany`, which can differ from one Mac to another. The maintainer's other app, favdir, already publishes a notarised disk image with a GitHub workflow.

## Decision

- The bundle identifier is `com.rangelreale.mirante`, as favdir's is `com.rangelreale.favdir`.
- The release build is universal, for Apple silicon and Intel. Qt's own libraries are already universal, so only Mirante's code is compiled twice.
- The cache variable `MIRANTE_MACOS_DISTRIBUTION_IDENTITY` names the certificate that signs the installed bundle. With it, the install signs with the hardened runtime and a secure timestamp, and gives `QtWebEngineProcess` the entitlements `com.apple.security.cs.allow-jit`, `com.apple.security.cs.allow-unsigned-executable-memory` and `com.apple.security.cs.disable-library-validation`. The app itself has no entitlements. Without the variable, the install signs ad hoc, as CI does.
- `tools/release-macos.sh` builds, tests, signs, makes the disk image, notarises it and staples the ticket, as favdir's script does. The release workflow runs it, and the maintainer can run it on a Mac.
- The signing secrets live in the GitHub environment `release`, which admits only version tags. The jobs that hold them pin their actions to commits.

## Alternatives

- **Apple silicon only.** It saves only compile time, since Qt WebEngine's framework stays universal unless it is thinned, and it would leave Intel Macs out.
- **Sign with `--deep` only.** `--deep` cannot give the helper its own entitlements.
- **Keep Qt's made-up identifier.** The keychain and macOS's privacy settings know an app by its identifier, so it must not change between machines.

## Consequences

- On the maintainer's Mac, the first build with the new identifier asks once more for keychain access to each token, as after a change of certificate ([decision 0090](0090-sign-the-macos-bundle.md)).
- A release needs the Developer ID certificate and an App Store Connect API key as secrets in the `release` environment. favdir's secrets can be copied.
- Ad hoc signed bundles from CI open only after **Open Anyway** in the Privacy & Security settings.
