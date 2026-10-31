# 0105. The sample data holds only items that work

- Status: Accepted
- Date: 2026-10-31

## Context

The sample data opens on first start, and it is also the main test fixture. To test items that the app cannot show, it held `timetrack.entry`, from a module that does not exist, and `claude.session.local`, a type that [decision 0084](0084-no-local-claude-sessions.md) removed. New users saw "This item type is no longer available" and "This item needs the timetrack module" on the Release dashboard.

## Decision

- Every item in the sample data has a type that a build with all modules provides.
- The three items of those types become notes. They keep their ids, so layouts and tests that refer to them stay valid.
- The `timetrack` connection and project settings leave the sample data too.
- The folder `tests/core/fixtures/missing-module` holds that data for the load and write-back tests. The old group file in `tests/core/fixtures/legacy-profiles` loses its `timetrack` connection, so that it still migrates to the sample data. The migration test keeps a connection of another kind in its own data.
- `tst_store_load` fails if the sample data gets an item from an unknown module or a removed type.

## Alternatives

- **Keep the data and hide those panels.** The sample data would still hold items that no one can use.
- **Copy the fixture for the tests at build time and add the data there.** It keeps all test data in one place, but the build gets more complex for a small folder.

## Consequences

- Adding a module means adding its id to the list in `tst_store_load` before the sample data can use it.
- The sample data still shows panels that say a token or server is not set, and a Claude login page on the Morning dashboard. This decision does not change those.
