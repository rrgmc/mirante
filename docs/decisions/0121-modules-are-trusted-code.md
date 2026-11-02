# 0121. Modules are trusted code, linked statically, without a sandbox

- Status: Accepted
- Date: 2026-11-02

## Context

Custom builds can include modules from other people's repositories. A module is compiled C++ in the app's process, so it can read the process's memory, including loaded tokens, find every widget through Qt, open files, start programs, use the network and call the keychain itself. The module interface is a convenience, not a wall. Storing modules as DLLs was also considered, for better organisation.

## Decision

- **Modules are trusted, and the documentation says so.** A module has the same access as the app, so a builder includes only modules they trust. This is how VS Code extensions and Obsidian plugins work too.
- **No sandbox.**
- **Modules stay static libraries,** as in [decision 0013](0013-compiled-in-modules.md).
- **Accidental leaks are prevented.** Most real incidents come from bugs, so:
  - item code never needs to see a token;
  - a token goes only to the origin of its server. See [issue 14](https://github.com/rrgmc/mirante/issues/14), for requests and redirects;
  - raw token access is removed from the interface. See [decision 0119](0119-module-interface-versions-and-checks.md);
  - tokens must not reach the log window.
- **The build is protected:**
  - other people's modules are pinned to commits;
  - an update's pull request links to the diff of each module;
  - the build job holds no signing secrets, because a module's CMake code runs any command it likes;
  - a scan reports which modules use sensitive APIs, such as `QProcess`, `QLibrary`, the keychain or their own network access. A malicious author can evade it, so it only helps a review.
- **Damage is limited:**
  - a custom build has its own identity. See [decision 0118](0118-custom-builds-are-their-own-app.md);
  - the documentation recommends narrow, read-only tokens where a service allows them;
  - each module's web pages get their own origin, in place of the shared `qrc:` origin. This one is a real barrier, because web content runs in a separate renderer process.

## Alternatives

- **Modules in a separate, sandboxed process.** This is a real barrier, but `QWidget` screens cannot cross processes, so it rules out the flexibility of [decision 0120](0120-fields-are-a-shortcut.md).
- **A restricted runtime, such as WebAssembly or JavaScript.** It could enforce permissions, but modules could no longer be ordinary Qt code.
- **DLLs built together with the app.** A user would see the same app, and the reasons [decision 0013](0013-compiled-in-modules.md) gave against plugins mostly go away when everything is built together. But the view kit, exports, module tests and all packaging would have to change, the installed size would probably grow, and a single module still could not be replaced safely. There is no need for it now, and the source is equally organised, since each module is already its own folder and library.
- **Hiding more from modules.** It would not stop an attacker, and it would make honest modules harder to write.

## Consequences

- The safety of a build rests on the builder's choice of modules and on reviewing updates.
- A module is no more dangerous than any other program the user runs, but also no safer.
- Nothing here prevents DLLs later. The changes they need are listed above.
