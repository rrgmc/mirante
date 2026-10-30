# 0077. No macOS in CI during development

- Status: Accepted
- Date: 2026-10-30

## Context

[Decision 0015](0015-app-skeleton-instead-of-spike.md) built every push on Windows, macOS and Linux. GitHub's macOS runners are sometimes unavailable. A job then waits and is cancelled without running a step, so the whole run shows as failed for a reason that has nothing to do with the code.

## Decision

- CI builds and tests on Windows and Linux only during development.
- The user builds on macOS by hand from time to time.
- The workflow keeps its macOS steps, so adding `macos-14` to its matrix turns them back on. M10, which packages the app, turns it on again.

## Alternatives

- **Keep macOS, and ignore its cancelled runs.** A red run would no longer mean anything, and real failures would be easy to miss.
- **Run macOS only on a schedule or by hand in GitHub.** It would still fail when no runner is free, and the manual builds cover the need for now.

## Consequences

- A change that breaks only macOS is found at the next manual build, not at the push. macOS-specific code, such as the tray's native status item, is the part most at risk.
- A green CI run no longer says anything about macOS.
