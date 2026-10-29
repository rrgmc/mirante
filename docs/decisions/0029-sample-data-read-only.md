# 0029. The sample data is read-only

- Status: Proposed
- Date: 2026-10-28

## Context

The app opens the sample data, compiled in as a resource, when there is no workspace. From M3 every edit is saved. A resource cannot be written.

## Decision

- The sample data can be edited, but nothing is saved. Changes are lost when the app closes or another workspace opens.
- While the sample data is open, the warning bar says that changes are not saved, and points to File > New Workspace….

## Alternatives

- **Copy the sample data into a new folder on the first edit.** The user would get a workspace they did not ask for, in a place they did not choose.
- **Refuse every edit to the sample data.** Trying the app would then mean making a workspace first.

## Consequences

- The Store has a read-only mode, which tests also use.
