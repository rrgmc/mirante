# Code plan: custom builds

This plan builds [custom builds](../roadmap.md#future-custom-builds). When it is finished, anyone can make a Mirante with the modules they choose, from a small repository that holds a manifest and a short workflow, and CI makes its packages. The official app is built the same way. See decisions [0116](../decisions/0116-custom-builds-in-separate-repositories.md), [0117](../decisions/0117-app-repository-and-json-manifest.md), [0118](../decisions/0118-all-builds-are-one-app.md), [0119](../decisions/0119-module-interface-versions-and-checks.md) and [0121](../decisions/0121-modules-are-trusted-code.md), and [the custom builds research](../research/custom-builds.md).

Nothing in this plan was tried yet. Part 2 starts by testing the CMake behaviour that the plan relies on.

## Where the work happens

By decision 0116, this repository stays as it is until the user approves the result. It takes only bug and security fixes, and each fix is carried to the new repositories by hand. This plan and the roadmap are the only changes here. All the code below is written in the new repositories, each with its own pull requests and CI.

| Repository | Holds |
|---|---|
| `mirante-core` | The core, the module interface, `main.cpp`, resources, packaging, tools, the reusable workflow, and the core and interface documents and decision records |
| `mirante-modules` | The nine first-party modules, each in a folder at the top of the repository |
| `mirante-app` | The sample: a manifest, a short `CMakeLists.txt`, a short workflow, a licence and a README |

## Decisions

Three new decisions are written with the code, in `mirante-core`:

- **Decision 0124: the core owns the app.** Decision 0117 left open who owns `main.cpp`, resources, packaging and the translation of the app's own texts. The core owns all of them. Decision 0118 makes every build the same app with the same names, so an app repository has nothing to configure. It holds only what differs between builds: the module list, the pins and the version. The alternative, an app repository with its own `main.cpp` and resources, would give each custom build a copy to keep up to date.
- **Decision 0125: how modules are fetched.** It records the choices under "Fetching" below, and the exact fields of the manifest.
- **Decision 0126: macOS signing is a separate job.** It supersedes the part of [decision 0101](../decisions/0101-macos-release-signing.md) in which one job builds and signs. See part 6.

## The manifest

The manifest is `mirante.json`, at the top of the app repository:

```json
{
  "qt": ["Pdf", "PdfWidgets"],
  "core": { "repo": "https://github.com/rrgmc/mirante-core", "ref": "v0.7.0" },
  "modules": [
    {
      "name": "mirante_modules",
      "repo": "https://github.com/rrgmc/mirante-modules",
      "ref": "v0.7.0",
      "folders": ["claude", "document", "folder", "github", "gitlab", "image", "jira", "notes", "web"]
    }
  ]
}
```

- **`qt`** lists the Qt modules that the selected modules need beyond what the core needs. Today only the document module needs `Pdf` and `PdfWidgets`, so the core stops asking for them.
- **`name`** is the name given to FetchContent, and so the name in `FETCHCONTENT_SOURCE_DIR_<NAME>`. It is written out because two owners can have repositories with the same name.
- **`folders`** are folders of that repository, and `"."` means its top. A listed folder is a built module. A folder that is not listed is not added at all, so its CMake code does not run.
- **Unknown fields stop configuration.** A misspelt `folder` would otherwise build nothing without saying so.

`DEFAULT OFF` has no effect with a manifest, since being listed is the choice. A module whose `DEPENDS` names a folder that is not listed stops configuration, with the existing message.

## Fetching

The app's `CMakeLists.txt` is short, and the template holds it unchanged:

```cmake
cmake_minimum_required(VERSION 3.21)

set(MIRANTE_VERSION "0.7.0" CACHE STRING "This app's version, as X.Y.Z")
project(mirante VERSION ${MIRANTE_VERSION} LANGUAGES CXX)
enable_testing()

set(MIRANTE_MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/mirante.json")
file(READ "${MIRANTE_MANIFEST}" manifest)
string(JSON core_repo GET "${manifest}" core repo)
string(JSON core_ref GET "${manifest}" core ref)

include(FetchContent)
FetchContent_Declare(mirante_core GIT_REPOSITORY "${core_repo}" GIT_TAG "${core_ref}")
FetchContent_MakeAvailable(mirante_core)
```

These choices follow from how CMake works, and part 2 tests each of them:

- **`enable_testing()` is in the app.** `ctest` in the top build folder finds only tests below a top folder that called it.
- **The core fetches the modules from inside its own directory scope,** where `src/modules` is added today. `find_package(Qt6)` makes imported targets that only that folder and those below it can see, so a module added from the app's scope would not find `Qt6::Core`. It also keeps the order: the module interface exists before the first module, and `mirante_finish_modules()` runs after the last one.
- **A module repository's top `CMakeLists.txt` never runs in an app.** `FetchContent_MakeAvailable` would run it, so each module repository is declared with a `SOURCE_SUBDIR` that does not exist, and the core calls `add_subdirectory()` for each listed folder. That top file exists for the module repository's own CI.
- **Paths use `MIRANTE_CORE_DIR`.** Functions such as `mirante_add_test()` and `mirante_precompile_headers()` run in a module's scope, and a module folder with its own `project()` changes `PROJECT_SOURCE_DIR`. The core sets `MIRANTE_CORE_DIR` once, as an internal cache variable, and every use of `PROJECT_SOURCE_DIR` in `cmake/` and `src/` changes to it.
- **The install and CPack use `CMAKE_BINARY_DIR`.** `CPACK_INSTALL_CMAKE_PROJECTS` and `CPACK_DEBIAN_PACKAGE_SHLIBDEPS_PRIVATE_DIRS` in `cmake/MiranteInstall.cmake` must name the top build folder, not the core's folder inside it.
- **A manifest is optional for the core.** Without `MIRANTE_MANIFEST`, the core builds with no modules. That is the `no-modules` build of today, and the core's own development build uses `ci/modules.json` (part 3).

The version stays the app's. The core has its own `project()`, so its `PROJECT_VERSION` no longer applies. It reads `MIRANTE_VERSION`, and `cmake/Version.h.in` uses that. `PROJECT_DESCRIPTION` and `PROJECT_HOMEPAGE_URL` become fixed values in the core, because every build is the same app.

## Part 1: Create and extract the repositories

This part creates public repositories, so it starts only after the user confirms it.

1. Tag the commit of `mirante` that the extraction starts from, as a record of where the repositories drift from.
2. Make `mirante-core` with `git filter-repo`, keeping the history, with `src/modules/` removed. A search found no core code or test that links a module. The core only uses module ids as text, such as `modules/folder/editor` in `tst_device_settings`.
3. Make `mirante-modules` with `git filter-repo --subdirectory-filter src/modules`, keeping the history.
4. Make `mirante-app` empty.
5. Give each repository the MIT `LICENSE`, its own `CLAUDE.md` and README, and branch protection that requires CI, as in [decision 0109](../decisions/0109-pull-requests-into-master.md). `mirante-core` also moves `docs/` by subject, as decision 0116 says.

`mirante-modules` and `mirante-app` have no build yet at the end of this part, so their required checks are set in part 3.

What the extraction changes in `mirante-core`, so that its CI passes from the first push:

- **No version tags are copied.** A pushed tag `vX.Y.Z` would start `release.yml`, so the extraction deletes the tags, and `release.yml` leaves the core. The core makes no releases of its own. Its tags only give apps something to pin.
- **`src/modules` leaves `src/CMakeLists.txt`,** so the core builds with no modules until part 2.
- **`Qt6Pdf` leaves `tools/check-package.py`,** because nothing links it without the document module. Part 4 makes this list follow from the manifest.
- **The translation files are regenerated,** so the module texts leave them, and CI's check stays green. Modules show in English until part 5. Part 5 takes their translated texts from `mirante_pt_BR.ts` in `mirante`, at the tag from step 1.
- **Links to `src/modules/` in the core's documents are broken** until part 3, except in the README, which now points at `mirante-modules`.

## Part 2: The core builds with outside modules

Before the real change, a small test project confirms the CMake choices under "Fetching", using the stub interface of `tests/cmake/depends/`. It runs as a CTest test in `mirante-core`, next to the `DEPENDS` test.

Then, in `mirante-core`:

- `cmake/MiranteManifest.cmake`: `mirante_read_manifest()` checks the fields and fetches each repository. `mirante_add_manifest_modules()` adds the listed folders, with a binary folder named `modules/<name>/<folder>`.
- `src/modules/CMakeLists.txt` goes, and `src/CMakeLists.txt` calls those functions instead.
- `MIRANTE_CORE_DIR` and `CMAKE_BINARY_DIR` replace the paths named above.
- `mirante_finish_modules()` reads `cmake/enabled_modules.cpp.in` from `MIRANTE_CORE_DIR`.
- `option(MIRANTE_CORE_TESTS)` is on when the core is the top project, and off in an app. Module tests are always added. `tests/support/OffscreenByDefault.cpp` stays available, since they need it.
- Tests that fetch nothing still pass: the fetch tests use local folders with `FETCHCONTENT_SOURCE_DIR_<NAME>`, so CTest needs no network.

`lupdate` no longer reads modules, so module texts leave the core's translation files in part 5. Until then, the core's translation check runs with the modules' files present.

## Part 3: The modules and the sample build

- **`mirante-modules`** gets a top `CMakeLists.txt` for its own CI, which fetches the core and builds all folders, and a `tools/check-module-includes.py` run. The script moves to the core and takes the folder to scan as an argument, so it also covers external modules.
- **`mirante-core`** gets `ci/modules.json`, a manifest that pins `mirante-modules` to a commit. The core's CI builds that manifest, so the split does not hide a break, as decision 0119 asks. A pull request that breaks the modules points the file at a branch of `mirante-modules` that holds the fix. The order for an interface change is therefore: a branch in `mirante-modules`, the core's pull request with the pin on that branch, a tag of the core, the modules' pull request, a tag of the modules, and the app's pins. Part 7 measures this cost.
- **`mirante-app`** gets `mirante.json` and the `CMakeLists.txt` above. Local work uses `-DFETCHCONTENT_SOURCE_DIR_MIRANTE_CORE=<folder>`, and `BUILDING.md` in the core says how.
- At the end of this part, `mirante-app` builds, tests and installs on Windows, and on Linux in CI.

## Part 4: Interface version, checks and Qt modules

**Interface version** (decision 0119):

- `src/module_api/mirante/ModuleApiVersion.h` holds `MIRANTE_MODULE_API_VERSION_MAJOR` and `_MINOR`. CMake reads them from this one file, as `MIRANTE_MODULE_API_VERSION`.
- `mirante_add_module()` gets a required `API 0.1` argument, and all first-party modules state it. A mismatch stops configuration with a message that names the module and points to the interface changelog.
- `src/module_api/CHANGELOG.md` is the changelog. A CI job fails if a pull request changes `src/module_api` without changing it.

**The interface configures on its own:**

- `cmake/MiranteModule.cmake` and the helpers it needs from `cmake/MiranteTargets.cmake` move to `src/module_api/cmake/`. `src/module_api/CMakeLists.txt` finds Qt when it is the top project.
- A module repository can then fetch `mirante-core` with `SOURCE_SUBDIR src/module_api` and compile and test alone. `mirante_finish_modules()` stays in the core.

**Compiler checks:**

- Modules are built with `-Werror=suggest-override` on GCC and Clang. `mirante_set_warnings()` uses `MIRANTE_WERROR` only for targets of the core and of `mirante-modules`, through a `MIRANTE_MODULES_WERROR` option that an app leaves off.
- `tests/module_minimal/` in the core is a small module that implements only the required methods. CI builds it, and it stays unchanged as the interface grows.
- The reusable workflow builds modules with `MIRANTE_PCH=OFF`, as `ci.yml` already does ([decision 0115](../decisions/0115-ci-builds-without-precompiled-headers.md)), so a missing include is found.

**Qt modules from the manifest:**

- `find_package(Qt6 ...)` asks for the core's list and the manifest's `qt` list. `Pdf` and `PdfWidgets` leave the core's list.
- `mirante_add_module()` stops configuration if `QT_LIBS` names a Qt module outside both lists. A module built with no manifest takes the list from its own top `CMakeLists.txt`.
- `tools/setup-qt.sh` and `tools/setup-qt.ps1` take the list as an argument. The archive names and the marker files follow from a table in the script that maps a Qt module to its archive, for example `PdfWidgets` to `qtpdf`.
- `tools/check-package.py` takes the list too. The list is also part of the Qt cache key.
- Whether the deploy tools copy a Qt module that the core does not link was not tested. The sample tests it with `Pdf`, which the core no longer links.

## Part 5: Module translations and the About dialog

**Translations:**

- `mirante_add_module()` compiles a module's `translations/<id>_<language>.ts` files, where they exist, into its resources under `/i18n`.
- `main.cpp` installs a translator for each enabled module. A module with no file for the language shows English, without a warning.
- A script splits today's `mirante_pt_BR.ts` once, by running `lupdate` on each module and copying the translated texts by context and source text. The texts stay the same.
- `tools/check-translations.py` and `tools/translation-glossary.py` take a folder, so they run in each repository. The glossary and the translation rules live in the core, and `mirante-modules` runs the core's copy from its pin.

**About dialog:** the core writes each module's repository and commit into `enabled_modules.cpp` at configure time, with `git rev-parse` on the fetched folder. A folder from `FETCHCONTENT_SOURCE_DIR_<NAME>` adds "local". The dialog lists them. Decision 0118 relies on this to tell builds apart.

## Part 6: The reusable workflow and signing

`mirante-core` publishes `.github/workflows/build-app.yml`, and `ci.yml` and `release.yml` use the same jobs where they can. An app's workflow is short:

```yaml
jobs:
  build:
    uses: rrgmc/mirante-core/.github/workflows/build-app.yml@v0.7.0
```

- **The core's pin is then in two places,** since GitHub needs a literal ref after `uses:`. `tools/check-app-pins.py` fails the build if the workflow's ref and the manifest's core ref differ. The update job in part 8 changes both.
- **The workflow reads the manifest first** with Python, checks out the core at its pin, gets Qt from the manifest's list, and passes the core's folder with `FETCHCONTENT_SOURCE_DIR_MIRANTE_CORE`, so the core is not cloned twice.
- **The build jobs hold no secrets,** on all three platforms. They build, test and package, as `ci.yml` does today.

**macOS has two jobs:**

1. **Build job, with no secrets.** It builds the universal bundle, tests it, installs it and runs `tools/check-package.py`. The install signs ad hoc, as it does today. It then packs the bundle with `tar` and uploads that. The packing matters: `actions/upload-artifact` makes a zip, which loses the symbolic links inside Qt's frameworks.
2. **Signing job, with the secrets, in the `release` environment.** It checks out the core at its pin, downloads the bundle, and runs only `tools/sign-macos.sh`, a new `tools/package-macos.sh`, `notarytool` and `stapler`. It reads no cache and starts nothing from the bundle.

`tools/package-macos.sh` makes the disk image with `hdiutil`, from the bundle and a link to `/Applications`. `cpack` cannot be used in the signing job, because it would run the build's install scripts. The Windows and Linux packages keep CPack. A check compares the two macOS disk image layouts, since the name and contents must stay as they are in a release today.

**Signing in the sample:** by decision 0116, `mirante-app` holds no secrets and publishes unsigned prereleases. A workflow input `macos-signing` is `none` or `developer-id`. With `none`, the signing job still runs, with no environment or secrets, and signs ad hoc. That tests the separate job's path on every sample build. `developer-id` is first tried in `mirante` itself, on a prerelease tag, when it moves to this workflow at the end. The `release` environment admits only tags, and this does not change. Whether a called workflow's job reads its environment's secrets through `secrets: inherit` is tested at that point.

**Prereleases:** a tag `vX.Y.Z-rc.N` in the app publishes a prerelease. The version check in `release.yml` accepts that form, and `MIRANTE_VERSION` gets the `X.Y.Z` part.

## Part 7: The first proof, and the point of decision

At this point, `mirante-app` builds, tests and packages all modules on Windows, macOS and Linux, which is the first approval condition. Then:

- **An interface change is carried across the repositories,** by the order in part 3. A real one is preferred. The cost is written down for the user.
- **The user starts using the sample** for their own work, as decision 0116 describes: run from its build with the options for the settings file, the data folder and the web data folder, and with a copy of the workspace. The user sets the length of this period here. It still reads the real tokens from the keychain.

The user can stop here and keep `mirante` as it is. The work in part 8 has value only if the split stays.

## Part 8: What outside authors need

- **The template.** A template repository `mirante-app-template`, with the manifest, `CMakeLists.txt`, workflow and README.
- **Automatic updates.** A scheduled job in the template looks for newer tags, or newer commits for a commit pin, and opens a pull request with a link to each diff, changing both core pins. It needs a GitHub App token or a personal access token, because a pull request opened with the workflow's own token starts no CI, and the README says how to set one up. Merging publishes a release. A failed update leaves the last release in place.
- **Sensitive API scan.** `tools/scan-module-apis.py` reports which modules use the keychain, the network, processes and web storage, as a help for reviews. It reports and never fails the build.
- **Web page origins.** Each module's web pages get their own origin, in place of the shared `qrc:` origin. No module uses `qrc:` pages in code today, so this is a change to the core's scheme handling. See [the research on web content in modules](../research/module-web-content.md). This part writes its own detail when it starts.
- **Documentation.** How to choose an id, what a third-party module can reach, narrow tokens, licences, and unsigned builds, as the roadmap lists. A person who has not seen the code then builds one module by following only that documentation, which is the third approval condition.

## Part 9: Replacing `mirante`

After the user approves, and only then:

1. The code of `mirante-app` replaces the code of `mirante`, in a pull request.
2. `mirante` moves its signing secrets to the signing job, and a prerelease tag proves the Developer ID path.
3. Decision 0013 and the roadmap are updated. `mirante-app` is archived or deleted.

## What is not checked

- macOS changes are checked by the CI jobs of `mirante-app` and `mirante-core`. The user builds by hand from time to time, as now.
- The Developer ID path of the signing job can be proved only in `mirante`, in part 9.
- Whether Windows paths stay short enough in a build folder such as `build/ci/_deps/mirante_core-src` is not known. Qt's generated files have long names. Part 2 finds out.
