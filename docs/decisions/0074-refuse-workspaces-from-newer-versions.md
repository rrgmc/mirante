# 0074. A workspace from a newer version is not opened

- Status: Accepted
- Date: 2026-10-30

## Context

The data folder syncs between devices, which may run different versions of the app. Until now, a file with a newer `schemaVersion` was skipped and reported, and the rest of the workspace opened. The user then works on part of the data. A group that cannot be read also takes its projects with it, so whole branches of the sidebar can be missing.

## Decision

- **Before opening,** `store::newerFiles()` looks at `workspace.mirante` and every entity file. If any has a newer `schemaVersion`, the window does not open the workspace, and the warning bar says to update Mirante. Conflict copies do not count.
- **What was open stays open.** At the start, with nothing open, the window stays empty, and nothing is written. The refused workspace is not remembered as the last one, but stays in the recent list if it was there.
- **While a workspace is open,** a newer file that arrives by sync is still skipped, reported and never written over, as before. The workspace is not closed under the user, who may be in the middle of something.

## Alternatives

- **Check only `workspace.mirante`.** Cheaper, but files sync one by one, and an entity file can arrive before it.
- **Open it read-only.** The user could still look, but part of the data would be missing, and read-only mode has so far meant only the sample data.
- **Close the workspace when a newer file arrives.** Safer for the data, which is already protected, and much more disruptive.

## Consequences

- A device that has fallen behind must update before it can open the workspace.
- Opening a workspace reads every entity file one extra time.
