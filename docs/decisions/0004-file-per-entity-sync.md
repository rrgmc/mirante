# 0004. One JSON file per entity in an externally synced folder

- Status: Accepted
- Date: 2026-10-28

## Context

The same groups, projects, tasks and dashboards should appear on several devices, without running a server. The data is small and is rarely edited on two devices at the same time.

## Decision

Store each group, project, task and root dashboard as its own JSON file in a folder the user chooses. An external tool such as Syncthing, OneDrive or Dropbox syncs the folder. The app watches the folder and reloads changed files.

## Alternatives

- **A private Git repository.** It gives history, but the app would have to commit, pull and resolve merges.
- **A single SQLite database.** Sync tools copy it whole and can damage it.
- **Peer-to-peer sync with a CRDT library such as Automerge.** It merges concurrent edits automatically, but it is much more work, and it only helps when the same task is edited on two devices at once.
- **Storing the data in Jira or GitLab.** It needs no sync, but it puts personal organisation into work tools.

## Consequences

- Conflicts are rare, and when they happen the sync tool keeps a conflict copy. The app detects these copies and lets the user choose. See [sync](../sync.md#conflict-copies).
- Device-specific data, such as folder paths, window positions, tokens and login profiles, stays outside the folder.
- Moving to a CRDT later would need a new decision.
