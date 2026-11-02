# 0116. Custom builds split the core, the modules and the app into repositories, tried first as a sample

- Status: Accepted
- Date: 2026-11-02

## Context

[Custom builds](../roadmap.md#future-custom-builds) let anyone build their own Mirante with the modules they choose. The roadmap kept the core and the first-party modules in this repository, and left a split for later. The user wants the official app to be built the same way as anyone's custom build, and wants to try that without risking the current repository.

## Decision

- **Three new public repositories, all under the MIT License:**
  - `mirante-core` holds the core and the module interface. It is extracted from this repository with its history.
  - `mirante-modules` holds all first-party modules.
  - `mirante-app` is the sample: the main app, which fetches the core and the modules. It holds no signing secrets, and its test packages are unsigned prereleases.
- **`mirante` takes only bug and security fixes during the sample.** Each fix is carried to the new repositories by hand. New features wait.
- **The final state.** If the user approves, the code of `mirante-app` replaces the code in `mirante`, which then pins `mirante-core` and `mirante-modules` as any custom build does. `mirante-app` is then archived or deleted.
- **The sample stays away from real data.** It has the same names as Mirante, by [decision 0118](0118-all-builds-are-one-app.md). So for daily use it is run from its build, not installed, with the existing options for the settings file, the data folder and the web data folder, and with a copy of the workspace. It still reads the real tokens from the keychain, and could change them if they are edited in the sample.
- **The user approves when all of these hold:**
  - `mirante-app` builds, tests and packages all modules on Windows, macOS and Linux;
  - at least one change to the module interface has been carried across the repositories, and the user finds the effort acceptable;
  - one module has been built by following the documentation alone, without changing the core;
  - the user has used the sample app for their own work for a period, whose length is set when the sample starts.
- **Documentation is split by subject.** Core and module interface documents and the existing decision records move to `mirante-core`. App and packaging documents stay with the app.

## Alternatives

- **Keep the core and first-party modules together, as the roadmap said.** One commit could then change the interface and every module. But the official app would not be built like a custom build, so the process would not be tested by its own project.
- **Keep the first-party modules inside `mirante-app`.** This needs the fewest repositories, but those modules would not be fetched, so the sample would not test fetching.
- **One repository per first-party module.** This is closest to a third-party module, but an interface change could touch up to ten repositories while the interface still changes often.
- **Freeze `mirante` completely.** There would be no drift, but security fixes such as [issue 14](https://github.com/rrgmc/mirante/issues/14) would wait for the approval.
- **Keep developing in `mirante`.** Every feature would then have to be synced, and the sample would have to stay short.
- **Private repositories.** Windows and macOS builds would use up a limited allowance of CI minutes, FetchContent would need an access token, and a free account has no branch protection on them.

## Consequences

- A change to the module interface takes a commit in `mirante-core`, a commit in `mirante-modules` and a pin update in the app. The sample measures this cost.
- `mirante` and the new repositories drift for as long as the sample runs, even with fixes only.
- Until the approval, this repository and [decision 0013](0013-compiled-in-modules.md) stay as they are, including the folder `src/modules/`.
- Each new repository needs its own `LICENSE` file, CI and branch protection.
