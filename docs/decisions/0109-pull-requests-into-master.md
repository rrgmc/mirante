# 0109. Changes reach `master` only through pull requests

- Status: Accepted
- Date: 2026-10-31

## Context

Until now, a session merged its work branch into `master` in the main checkout and pushed `master` when the user asked. CI ran only after the push, so a change that broke the Windows or Linux build was found after it was already on `master`. [Decision 0017](0017-parallel-tasks-with-fixed-interfaces.md) also had the orchestrating session make small changes on `master` directly.

## Decision

- **`master` is protected on GitHub.** Nobody, administrators included, can push to it, force-push it or delete it. A change reaches it only by merging a pull request.
- **CI must pass first.** A pull request can be merged only when the checks `build (windows-2022)`, `build (ubuntu-24.04)`, `no-modules` and `module-includes` have passed on a branch that is up to date with `master`. `build (macos-15)` runs but is not required, because GitHub's macOS runners are sometimes unavailable (see [decision 0077](0077-no-macos-in-ci-during-development.md)).
- **No approval is required.** The repository has one maintainer, and GitHub does not let authors approve their own pull requests.
- **Work branches are pushed.** A session pushes its branch, opens a pull request with `gh pr create`, and merges it with a merge commit once the checks pass. The branch is then deleted, locally and on GitHub.
- **The orchestrating session merges agents' branches** through one pull request each, one at a time. A change to a file that no task owns goes on a branch and a pull request too.

## Alternatives

- **Keep merging locally, protect nothing.** Fast, but broken builds reach `master` and are found only after the push.
- **Require all five checks.** A missing macOS runner would block every merge until it came back.
- **Squash merges.** One commit per pull request is tidier, but loses the focused commits that a branch is made of.
- **Let administrators bypass the rules.** The maintainer is the only administrator, so the protection would bind no one.

## Consequences

- Every change to `master` has been built and tested on Windows and Linux.
- A merge waits for CI, which takes several minutes. Sessions start the next piece of work on a new branch while they wait.
- With required up-to-date branches, a pull request must merge `master` in again whenever another one lands first.
- Pushing `master` by hand is no longer possible. Tags for releases are not affected.
