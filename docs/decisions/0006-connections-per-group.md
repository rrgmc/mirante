# 0006. API connections per group, tokens in the keychain

- Status: Superseded by [0054](0054-module-settings-replace-connections.md); earlier, in part, by [0050](0050-profiles-hold-logins-connections-and-settings.md)
- Date: 2026-10-28

## Context

Groups use different accounts and sometimes different servers: gitlab.com or a self-hosted GitLab, Jira Cloud or Jira Data Center. Items should not need their own credentials. Tokens are secrets and must not end up in a synced folder.

## Decision

- Each group has a list of connections. A connection has a kind, a base URL and, for Jira Cloud, an email.
- A group can have several connections of each kind, with one default.
- A project can override the default, and an item can override the project.
- Tokens are stored with qtkeychain in the operating system keychain, keyed by group and connection id. They are never synced.

## Alternatives

- **One global token per service.** It cannot handle separate accounts.
- **Exactly one connection of each kind per group.** It is simpler, but cannot handle a client's own Jira site inside the Work group.
- **Syncing encrypted tokens.** It saves entering them on each device, but needs key management.

## Consequences

- Each device asks for each token once.
- The status cache is keyed by connection, so data from different accounts is never mixed.
- Jira Cloud and Data Center need different authentication and API versions. See [integrations](../integrations.md#jira).
