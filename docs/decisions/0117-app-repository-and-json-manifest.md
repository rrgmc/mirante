# 0117. The app is the top-level project, and a JSON manifest pins what it fetches

- Status: Accepted
- Date: 2026-11-02

## Context

The roadmap had the core read the manifest and fetch the modules, so the core was the top-level CMake project. With the split of [decision 0116](0116-custom-builds-in-separate-repositories.md), the app is a project of its own. The format of the manifest and what a pin may name were still open.

## Decision

- **The app is the top-level CMake project.** It owns the manifest. A few lines in its `CMakeLists.txt` read the core's entry and fetch the core with FetchContent. A CMake function from the core then reads the rest of the manifest, fetches the module repositories, and maps the listed folders onto `MIRANTE_MODULES`.
- **The manifest is JSON.** CMake reads it with `string(JSON)`, and CI scripts can read it before Qt is installed.
- **A pin is a tag or a commit.** FetchContent's `GIT_TAG` takes either. Repositories of the app's own owner use tags. Other people's repositories use commits, since a tag can move, and a comment or a field can record the version.
- **The manifest lists the Qt modules.** The Qt setup scripts, the CI cache keys, `find_package` and the package check all follow from that list. Configuration stops if a module's `QT_LIBS` names a Qt module that the manifest does not list.
- **Local work uses CMake's `FETCHCONTENT_SOURCE_DIR_<NAME>`,** so a checkout on disk replaces a pinned repository without changing the manifest.

An example, whose exact fields the code plan settles:

```json
{
  "qt": ["WebEngineWidgets", "Pdf", "PdfWidgets"],
  "core": { "repo": "https://github.com/rrgmc/mirante-core", "ref": "v0.6.0" },
  "modules": [
    { "repo": "https://github.com/rrgmc/mirante-modules", "ref": "v0.6.0", "folders": ["gitlab", "web"] },
    { "repo": "https://github.com/someone/their-module", "ref": "a84f210", "folders": ["."] }
  ]
}
```

## Alternatives

- **The core as the top-level project.** The core would then have to know about apps, and an app repository could not own its resources and packaging as simply.
- **YAML or TOML.** They are easier to write by hand, but CMake has no reader for them.
- **A CMake file as the manifest.** It needs no parsing in CMake, but scripts could not read the Qt modules without running CMake, which needs Qt.
- **Commits only.** A commit cannot change, but a version is easier to read, and a pull request per release is less noise than one per commit.
- **Qt modules only in each module's declaration.** The modules would have to be fetched before Qt is installed.

## Consequences

- The core's CMake code can no longer assume that `PROJECT_SOURCE_DIR` is the core. Its own files are found through `CMAKE_CURRENT_FUNCTION_LIST_DIR` or similar.
- The split must settle who owns `main.cpp`, resources, packaging and the translation of the app's own texts.
- The scheduled update job looks for newer tags or commits, depending on each pin.
