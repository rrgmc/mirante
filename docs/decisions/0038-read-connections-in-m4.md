# 0038. Connections are read in M4, without tokens

- Status: Accepted
- Date: 2026-10-29

## Context

The GitLab and Jira page types store a project path and number, or an issue key. Their URL needs the base URL of a connection, such as `https://gitlab.com`. Connections, their dialogs and tokens were planned for M5, after the page types in M4.

## Decision

- M4 adds the [resolution order](../data-model.md#connection) and `ItemContext::connection()`, which returns the connection's details without a token.
- The group dialog gets a minimal connection list in M4: kind, name, base URL, and the group default for each kind. The kinds offered are those the item types of this build declare.
- Tokens, kind-specific fields, testing a connection, connection kinds registered by modules, and project overrides in the project dialog stay in M5.

## Alternatives

- **Resolution only, without a dialog.** Connections would be added by editing group files until M5.
- **All of M5's connection work in M4, without tokens.** A larger M4 and a smaller M5.
- **A full URL field on the page types for now.** Simple, but M5 would make the field redundant, and it would stay in synced data.

## Consequences

- The page types work in M4 with any GitLab or Jira host.
- M5 replaces the minimal list with dialogs built from each connection kind's fields. Connections saved in M4 keep working, because the data model does not change.
