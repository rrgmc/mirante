# 0010. Folders stored as identities, mapped to paths per device

- Status: Accepted
- Date: 2026-10-28

## Context

Projects have a main folder and possibly others, usually Git repositories. Project data is synced, but a path such as `F:\prog\backend` does not exist on a Mac.

## Decision

Synced data stores each folder as an identity: an id, a name and an optional Git remote URL. Each device maps the folder id to a local path in its own settings.

## Alternatives

- **One path per operating system in synced data.** It breaks when two Windows devices use different drives.
- **Folders only in device settings.** Then the project's folder list would not sync.

## Consequences

- A device without a mapping shows "Folder not set on this device" and lets the user pick one.
- The remote URL lets the app suggest the mapping, and match the folder to a GitLab project.
