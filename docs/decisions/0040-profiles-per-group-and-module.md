# 0040. One web login profile per group and module

- Status: Superseded by [0042](0042-user-managed-web-profiles.md)
- Date: 2026-10-29

## Context

[Decision 0005](0005-profiles-per-group.md) gives each group one web profile, shared by the web items of every module. Browsers keep each site's storage apart, so pages of different sites cannot read each other's cookies. Module code is a different matter. With `ItemContext::profile()`, kept by [decision 0037](0037-core-makes-web-views.md), a module receives the whole profile. Its cookie store reports the cookies of every site in it, and the module can load any page in it and run scripts there. A Jira module could read GitLab sessions.

## Decision

- Each module has its own profile in each group, with the storage name `g-<groupId>-<moduleId>`. The module is the prefix of the item type id, such as `gitlab` for `gitlab.mr.page`.
- The shared profile is also one per module, `shared-<moduleId>`. A host in a group's `profileOverrides` uses the shared profile of the item's module.
- `ItemContext::profile()` returns the profile of the item's own module, so module code can reach only the sessions of its own module.
- The `app` profile stays one, for built-in panels.
- Deleting a group deletes the profiles of all its modules, under the rule of [decision 0039](0039-profiles-deleted-only-by-local-delete.md).

This supersedes the profile part of decision 0005. Its other parts stand.

## Alternatives

- **One profile per group, as in 0005, without `profile()`.** One login per site and group, and module code sees no profile at all. But a module that needs full control of its page cannot have it.
- **One profile per group, with `profile()` logged and justified.** One login per site and group, but the separation between modules is a convention only.

## Consequences

- Module code cannot read another module's sessions or storage.
- The same site is logged in to once per module and group. A claude.ai page in a Web page item and a Claude session item need two logins. So does a gitlab.com page in a Web page item and a GitLab MR page item.
- Sign-in that passes through another domain, such as Atlassian ID or single sign-on through a company identity provider, is also repeated per module.
- A group has up to one profile for each module with web types: `web`, `claude`, `gitlab` and `jira` today. Each profile has its own data folder and cache.
