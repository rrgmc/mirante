# Custom builds after M10

- Status: Research. Nothing here is decided or built.
- Date: 2026-10-31

[Custom builds](../roadmap.md#future-custom-builds) let anyone build their own Mirante with the modules they choose, including modules from other people's repositories, and get its executables from CI. The roadmap entry was written on 30 October, before the packaging of M10 was merged. This document checks that entry against what M10 and the work after it built, and lists what the entry missed.

Nothing here was tested. Where a statement depends on behaviour that was not tried, it says so.

## Summary

| Topic | What changed since the entry | Effect on custom builds |
|---|---|---|
| [Packaging](#packaging) | `cmake --install` and CPack make all three packages | The prerequisite is met |
| [Signing](#signing) | The macOS job holds the secrets while it builds | Building and signing must become separate jobs |
| [App identity](#app-identity) | The bundle identifier, package name, keychain service and settings are fixed | A custom build must replace Mirante or stand beside it; not decided |
| [Translations](#translations) | One set of translation files for the whole app | External modules would not be translated |
| [Qt components](#further-qt-components) | Qt PDF showed every place a Qt module is named | The manifest must name the Qt modules |
| [Automatic updates](#automatic-updates) | Changes reach `master` only by pull request | A pull request opened by a workflow starts no CI |
| [Compiling](#compiling) | Precompiled headers and ccache | External modules need a build without precompiled headers |
| [Trust](#trust) | Research on web content in modules | Modules can read each other's web storage |

## Packaging

M10 is accepted. [Decision 0098](../decisions/0098-packages-from-cmake-install.md) makes the packages with `cmake --install --component Mirante` and CPack. Qt's deploy script, from `qt_generate_deploy_app_script()`, copies the Qt libraries next to the app on all three platforms. The release workflow, `.github/workflows/release.yml`, builds a tag on Windows, macOS and Linux and attaches the three packages to a GitHub release. See [releasing](../releasing.md).

What a custom build can reuse:

- `MIRANTE_MODULES` already selects an exact list of modules, and `mirante_finish_modules()` checks it and `DEPENDS` ([modules](../modules.md#building)). The manifest can map onto it.
- `tools/build.ps1 -Preset ci -Package`, `tools/setup-qt.sh` and `tools/check-package.py` already build, test and check a package in one call per platform.
- `MIRANTE_VERSION` sets the version from outside. An app repository needs its own version, so its tags, not the core's, should set it.

What does not fit yet:

- The workflow is written for this repository: it checks out one repository, and its cache keys and paths are fixed. The reusable workflow in the roadmap must take the manifest instead.
- The Qt version, 6.8.3, is written in both workflows. The pinned core commit therefore decides the Qt version of an app repository. That is acceptable, but it should be stated.

## Signing

The roadmap asks that signing secrets are not available while module code is configured or built. The current macOS job does not meet that. It runs in the `release` environment ([decision 0101](../decisions/0101-macos-release-signing.md)), imports the Developer ID certificate into a keychain, and then runs `tools/release-macos.sh`, which builds, tests, signs, notarises and staples in one go. Every build step can reach the keychain and the notarisation key.

For Mirante's own modules this is acceptable. For a module from someone else's repository it is not: its CMake code and its build steps run with the certificate available.

A custom build therefore needs two jobs on macOS:

1. A build job without secrets. It builds, tests and installs the universal bundle unsigned, and uploads it as an artefact.
2. A signing job in the `release` environment. It downloads the bundle, signs it with the hardened runtime and the entitlements of `QtWebEngineProcess`, makes the disk image, notarises it and staples the ticket. It runs no code from the build.

Today the install signs the bundle, by running `tools/sign-macos.sh` after `macdeployqt`. That script already takes a finished bundle, an identity and the entitlements, so the signing job can call it directly on the downloaded bundle. Mirante's own release could use the same two jobs, so there is one path.

The Windows setup and the `.deb` package are not signed, so they need no change. A builder without their own certificates gets unsigned packages on all three platforms, and an ad hoc signed bundle on macOS.

## App identity

Several names fix which app a build is:

| Name | Value | Where |
|---|---|---|
| macOS bundle identifier | `com.rrgmc.mirante` | `src/CMakeLists.txt`, [decision 0103](../decisions/0103-bundle-identifier-rrgmc.md) |
| Organisation and application name | `Mirante`, `Mirante` | `cmake/Version.h.in`; they place `QSettings` and the data folder |
| Keychain service | `mirante` | `src/api/KeychainBackend.cpp` |
| Package name and install folder | `mirante`, `/opt/mirante` | `cmake/MiranteInstall.cmake`, [decision 0099](../decisions/0099-linux-package-in-opt.md) |
| Desktop file name | `mirante` | `src/main.cpp` |

A custom build can keep these names or change them.

- **Keep them: a custom build replaces Mirante.** It reads the same settings, workspace and tokens. Installing it removes the official package, and the other way round. The macOS keychain asks again for each token, because the certificate differs. A workspace that uses item types of a module the build lacks must already cope with missing types.
- **Change them: a custom build is its own app.** It installs beside Mirante, with its own settings, workspace and tokens. The manifest would then give an app id and a display name, and every name above would follow from them. The tokens of one app are not visible to the other.

The second is safer, because a third-party module never sees the official app's tokens. The first is simpler for someone who wants Mirante with one more module. This needs a decision before the plan is written.

## Translations

`src/CMakeLists.txt` builds one set of translation files, `translations/mirante_en.ts` and `translations/mirante_pt_BR.ts`, from every source file under `src/`, and CI fails if they are out of date or incomplete ([decision 0086](../decisions/0086-english-and-brazilian-portuguese.md)). A module from outside the source tree is not read, so its texts would show in English in every language. If it were read, the core's translation files would change with each app repository.

Each module would need its own translation files, compiled into its resources, and the core would load a translator for each enabled module. The translation checks in CI would then run per module. The roadmap entry does not mention this.

## Further Qt components

`mirante_add_module()` takes `QT_LIBS` and links those Qt modules. Adding Qt PDF in M12 showed what else must change for a Qt module that the core does not use yet ([M12 plan](../plans/m12-document-items.md), part 1):

- the top-level `find_package(Qt6 ... COMPONENTS ...)`;
- the `-m` list and the marker file in `tools/setup-qt.ps1` and `tools/setup-qt.sh`;
- the Qt cache keys in `ci.yml` and `release.yml`;
- `tools/check-package.py`, which expects certain Qt libraries in the package.

Deploying them is probably already solved: Qt's deploy script, `windeployqt` and `macdeployqt` follow what the executable links. This was not tested with a Qt module the core does not link.

So the Qt modules must come from the manifest, or from each module's own declaration, before Qt is installed. The setup scripts, the cache key and `find_package` would all follow from that one list.

## Automatic updates

The roadmap asks for a scheduled job that opens a pull request when a pinned repository has newer commits, with CI building it and the merge publishing a release.

On GitHub, an event caused by a workflow's own `GITHUB_TOKEN` starts no other workflow, except `workflow_dispatch` and `repository_dispatch`. `release.yml` relies on this: it publishes the release itself, so no second workflow runs. For updates, it means:

- A pull request opened by the scheduled job with `GITHUB_TOKEN` gets no CI run, so it can never show green checks.
- A release published by a workflow with `GITHUB_TOKEN` starts no workflow that listens for releases.

The scheduled job needs a GitHub App token or a personal access token, or it must start the build itself with `workflow_dispatch`. The template should say which, and how the builder sets it up.

Since [decision 0109](../decisions/0109-pull-requests-into-master.md), Mirante's own `master` changes only through pull requests. An app repository made from the template could follow the same rule, and an automatic merge would then need the same token.

## Compiling

[Decision 0112](../decisions/0112-precompiled-headers.md) compiles every module with `cmake/MirantePrecompiled.h`, which holds the standard and Qt headers. A file that forgets an include still compiles. For a module in this repository, the `no-modules` job builds with `MIRANTE_PCH=OFF`, but only for the core. For an external module the header becomes part of the module interface without anyone deciding so: if the core removes a header from it, a module that relied on it stops compiling.

The reusable workflow should build the modules once with `MIRANTE_PCH=OFF`, or the module documentation should say that the precompiled header is not part of the interface. [Decision 0110](../decisions/0110-ci-runs-once-per-change-and-caches-compiling.md) adds ccache to CI, and the reusable workflow can use it in the same way.

`tools/check-module-includes.py` scans only `src/modules/`, as the roadmap already notes.

## Trust

The roadmap says that a third-party module runs in the app process and can read tokens and web logins. [The research on web content in modules](module-web-content.md) adds one more point: every `qrc:` page shares one origin, so the web pages of two modules can read each other's `localStorage`. The documentation for people who build should say this too.

## What should change in the roadmap

- The prerequisite is met: the entry builds on the packaging of M10.
- The signing criterion names the split into a build job without secrets and a signing job that runs no build code.
- A new criterion: the app identity of a custom build, decided before the plan.
- A new criterion: modules carry their own translations.
- The Qt components criterion says where the list of Qt modules comes from.
- The automatic update criterion names the token that lets a pull request start CI.
- A new criterion: external modules are checked without precompiled headers.
- The trust paragraph mentions the shared `qrc:` origin.
