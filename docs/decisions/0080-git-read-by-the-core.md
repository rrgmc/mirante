# 0080. The core reads Git state for modules

- Status: Accepted
- Date: 2026-10-30

## Context

Two modules need Git state. The `folder` module shows a folder's branch and a short `git status`. The `gitlab` module's CI Pipeline can follow the branch of the project's main folder, and must notice when the branch changes. Modules may use Qt, but [module rules](../modules.md#rules-for-module-code) put platform code in the core, and the status cache cannot watch files it does not know about.

## Decision

- **The branch comes from the files.** `GitHead` in the core reads `.git/HEAD`, following the `.git` file of a worktree. No process is started, so reading it is cheap enough for every status fetch.
- **The summary comes from `git status`.** `GitStatusReader` runs `git status --porcelain=v2 --branch` in the background, with a time limit, and parses the output. Without Git installed, the item says so.
- **Modules ask through the interfaces.** `ItemContext` gives `gitHead()` and `gitStatus()`. `StatusJob` gives `folderPath()` and `gitHead()`.
- **The status cache watches the heads a job read,** and fetches the item again when one changes. A branch switch in a terminal updates the pipeline without waiting for the interval.

## Alternatives

- **libgit2.** It reads status without a process, but adds a dependency to build on three platforms for one line of text.
- **Each module runs `git` itself.** Two modules would parse the same output, and the status cache could not know what to watch.
- **Polling the head at the status interval.** A branch switch would take up to the interval to show.

## Consequences

- The folder view needs Git on the `PATH` for its status line. The branch works without it.
- A head file that is replaced, as Git does when it switches branches, must be watched again after each change. The status cache does this.
