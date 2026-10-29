# 0005. One web login profile per group, with a shared option

- Status: Superseded by [0042](0042-user-managed-web-profiles.md); earlier, in part, by [0040](0040-profiles-per-group-and-module.md)
- Date: 2026-10-28

## Context

Different groups can use different accounts on the same site, for example a work and a personal claude.ai account. Logging in to every site once for every group can also be tedious for sites that use the same account everywhere.

## Decision

Each group has its own persistent `QWebEngineProfile`. A group can send individual site hosts to a shared profile instead.

## Alternatives

- **Strictly one profile per group for every site.** It is simpler, but means repeated logins for sites with one account.
- **Named profiles assigned by hand,** such as "Work" and "Personal". More flexible, but adds another concept to manage.

## Consequences

- Groups can use different accounts on the same site.
- Profiles separate storage only. All pages share the Chromium processes, so this is not a security boundary.
- Login profiles are not synced. The user logs in once per device and group.
