# 0039. A group's web profile is deleted only when the group is deleted on this device

- Status: Accepted
- Date: 2026-10-29

## Context

[Web views](../web-views.md#profiles) says that deleting a group deletes its profile. A group can also disappear because its file was removed by a sync tool. Sync tools sometimes remove and restore a file, for example while resolving a conflict. Deleting the profile then would log the user out of every site in that group.

## Decision

- The profile of a group is deleted when the user deletes the group in this app, on this device.
- A group removed by a reload, after an external change, keeps its profile folder.

## Alternatives

- **Delete on any removal.** Simple, but a short-lived removal by a sync tool loses logins.
- **Delete profiles of absent groups at start.** Cleans up after other devices, but has the same risk when a group file is missing at start.

## Consequences

- A group deleted on another device leaves its profile folder on this device. The folder holds cookies and cache only, and is never synced.
- Cleaning up such folders can be added later, for example from a settings page.
