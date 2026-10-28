# 0017. Parallel tasks with fixed interfaces and globbed sources

- Status: Accepted
- Date: 2026-10-28

## Context

The user wants to implement milestones with several agents working at the same time, each in its own worktree. The M2 plan first split the work into five steps, at most two at a time. Two things limit parallel work:

- A task cannot start until the code it calls exists, so most tasks wait for others.
- Every task that adds a file edits the same `CMakeLists.txt` lists. Branches that do this conflict when they are merged.

## Decision

- **Interfaces first.** The first task of a milestone writes every header the milestone plan names, with stub implementations that compile. The other tasks implement code behind those headers, so they can start as soon as their real dependencies are merged, and often at once.
- **Each task owns a list of files.** A task that needs a change to a file it does not own stops and reports it. The orchestrating session makes the change on `master`.
- **Sources and tests are found by globbing.** Each library compiles every `.cpp` in its folder with `file(GLOB ... CONFIGURE_DEPENDS)`. Each `tst_*.cpp` in a test folder becomes a test. Modules keep explicit file lists, because each module folder belongs to one task.
- **Agents do not merge.** An agent commits on its own branch and reports. The orchestrating session merges branches into `master` one at a time, after building and testing each one with `master` merged in.
- **A build script for any shell.** `tools/build.ps1` enters the MSVC developer environment itself, so an agent can build from a plain shell.

## Alternatives

- **Larger steps, as first planned.** Fewer merges, but at most two agents at a time, and each agent's work is too large to review as one piece.
- **Explicit source lists, each task editing them.** CMake's documentation prefers explicit lists. With many branches adding files to the same lists, merge conflicts are frequent. `CONFIGURE_DEPENDS` removes the main reason against globbing: new files are found without a manual reconfigure.
- **Agents merge their own branches.** Two agents merging into the main checkout at the same time can interfere. One merger keeps `master` building after every merge.

## Consequences

- The first task of each milestone is on the critical path. The user reviews its headers before the other tasks start, because a wrong interface affects every task.
- Header changes after the first task go through the orchestrator, which slows a task that finds a wrong interface.
- `CONFIGURE_DEPENDS` checks the folders on every build. The cost is small for a project of this size.
- A stray `.cpp` file in a library folder is compiled. Scratch files belong in the scratchpad, not in `src/`.
