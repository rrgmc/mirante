# 0111. Only the user merges pull requests

- Status: Accepted
- Date: 2026-10-31

## Context

[Decision 0109](0109-pull-requests-into-master.md) made pull requests the only way into `master`, and let a session merge its own pull request once the required checks passed. The user wants to decide when each change lands.

## Decision

- **A session never merges a pull request on its own.** When the required checks pass, it tells the user that the pull request is ready and stops.
- **The user merges.** The user merges on GitHub, asks a session for the merge, or types `/mmp`, which merges and cleans up.
- **The orchestrating session of a parallel milestone** opens one pull request per agent's branch and tells the user which order to merge them in. It no longer merges them. This replaces the merging part of [decision 0017](0017-parallel-tasks-with-fixed-interfaces.md).

## Alternatives

- **Keep merging without approval.** Faster during rapid development, but changes land without the user choosing when.
- **GitHub auto-merge.** A pull request would merge as soon as its checks pass, which is the same as a session merging it.

## Consequences

- Green pull requests wait for the user, and the worktrees behind them stay until the merge.
- A branch can fall behind `master` while it waits, and then needs `master` merged in and a new CI run before it can merge.
