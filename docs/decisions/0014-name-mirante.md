# 0014. Name the app Mirante

- Status: Accepted
- Date: 2026-10-28

## Context

The app was developed under the working name "rraidash", in a repository called `rraidash-poc1`. That name was temporary. The app needs a final name before code exists, because the name becomes the CMake project, the executable, C++ namespaces, CMake options, the keychain service and the data folder.

The name should be easy to read and say in English and in Portuguese, fit an app that shows all of one person's work in one place, and not be used by another dashboard or productivity tool.

## Decision

The app is called **Mirante**. In Portuguese, a mirante is a lookout point: a high place from which you see everything around you at once.

- In prose the name is written "Mirante".
- In identifiers it is lowercase: the namespace `mirante::`, the CMake function `mirante_add_module()`, the target `mirante::module_api` and the keychain service `mirante`.
- CMake options use the prefix `MIRANTE_`, for example `MIRANTE_MODULE_<ID>` and `MIRANTE_MODULES`. This replaces the `RRAIDASH_` prefix named in [decision 0013](0013-compiled-in-modules.md).

Decisions 0001 and 0013 still say "rraidash", because accepted records are not rewritten. Read that name as Mirante.

## Alternatives

About 50 names were checked against existing products and open-source projects.

- **Names ending in "dash"**, such as SuperDash, Omnidash, Deskdash, Tiledash, Wavedash and Panodash. Almost every one that is easy to say is already used, often by another dashboard.
- **Weavedash** and **Teladash** were free, but "weave" is hard to say in Portuguese, and Teladash is easily confused with Teledash.
- **Tessera** is used by many projects, including a workspace for AI coding sessions with tabs, panes and worktrees.
- **Telar** (loom) was a close second. Mirante describes the purpose of the app, an overview of all work, more directly.
- **Farol**, **Nodal** and **Vitral** are already used by dashboard or productivity tools.

## Consequences

- No dashboard or productivity tool uses the name. Unrelated apps called "Mirante" or "Mirantes" exist, such as condominium apps and an HR network, so the name is not unique in web searches.
- The repository directory and any remote must be renamed outside the code.
- The prototype stores its state under a new browser storage key, so state saved under the old key is not loaded.
