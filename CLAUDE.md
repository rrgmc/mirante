# Mirante

A personal "all work" desktop dashboard in Qt 6 (C++17, Qt Widgets, CMake) for Windows, macOS and Linux. Start with [docs/README.md](docs/README.md).

## Git workflow

Several sessions may work on this repository at the same time, so each piece of work happens in its own Git worktree.

- Before the first edit, create a worktree with its own branch (`EnterWorktree` in Claude Code, or `git worktree add -b <branch> <dir>`). Name the branch after the work, for example `m2-layout-engine`.
- Never edit files in the main checkout on `master` directly. It stays clean, so other sessions can branch from it.
- Commit as work progresses, one focused commit per finished piece.
- When the piece is finished and its build and tests pass, merge the branch into `master` in the main checkout, then remove the worktree and delete the branch. During rapid initial development this merge does not need the user's approval. This overrides the global rule that merging waits for the user.
- If `master` moved since the branch started, merge `master` into the branch and resolve conflicts there before merging back.
- Push `master` only when the user asks. Never push work branches.
- Split parallel work so that two worktrees rarely change the same files. For example, give each milestone part its own module.
- When agents implement a milestone in parallel, follow [decision 0017](docs/decisions/0017-parallel-tasks-with-fixed-interfaces.md) and the task table in the milestone's plan. Each agent commits on its own branch and does not merge. The orchestrating session merges the branches into `master` one at a time.

## Documentation

- The design lives in `docs/`. Read the relevant documents before changing behaviour, and update them in the same commit when the behaviour changes.
- Record each significant design decision as a new file in `docs/decisions/`, using the template in `docs/decisions/README.md`. Do not rewrite an accepted decision; add a new one that supersedes it.
- Write a detailed code plan for a milestone just before implementing it, not earlier. The milestones are in `docs/roadmap.md`.
- Use the terms defined in the glossary in `docs/README.md`.

## Building

- `task` lists the common commands, such as `task test` and `task check`. See `Taskfile.yml`.
- Qt 6.8 LTS with the WebEngine, WebChannel and Network modules.
- Windows uses MSVC 2022 64-bit. Qt WebEngine does not support MinGW.
- Development and verification happen on Windows. Changes must still build on macOS and Linux, so keep platform-specific code in `src/platform/`.
