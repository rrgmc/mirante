# 0042. Web profiles are a list the user manages

- Status: Accepted
- Date: 2026-10-29

## Context

[Decision 0040](0040-profiles-per-group-and-module.md) gives each module its own web profile in each group, and one shared profile per module. The same site then needs a login for every module and group. A claude.ai page in a Web page item and a Claude session item need two logins, and sign-in through a company identity provider is repeated too. The user found this tedious, and the per-host overrides of [decision 0005](0005-profiles-per-group.md) hard to follow.

The user wants to decide which items share logins, by name, such as "Work" and "Personal".

## Decision

- **A list in the workspace.** The workspace holds a list of web profiles, each with an id and a name, in the `webProfiles` key of `workspace.mirante`. The list syncs with the workspace.
- **Default.** A profile called Default, with the id `default`, always exists. It is not stored in the list. It cannot be renamed or deleted.
- **Managing.** File > Web Profiles… creates, renames and deletes profiles. Deleting one asks first.
- **Choosing.** Each group has a default profile, `webProfile` in the group file, set in the group dialog. Each item of a web type can choose its own, `webProfile` in the item. An item uses its own profile, else its group's, else Default. A step that names a profile not in the list, because it was deleted or has not arrived yet, is skipped.
- **Logins stay local.** The logins of a profile live under the app's local data folder, in `profiles/p-<id>`, and are never synced.
- **Deleting.** Deleting a profile on this device deletes its logins on this device, in the spirit of [decision 0039](0039-profiles-deleted-only-by-local-delete.md). Groups and items that name it are not changed; they fall back by the rule above. A profile that disappears through a sync keeps its logins. Deleting a group deletes no logins.
- **No module isolation.** `ItemContext::profile()` takes no URL and returns the item's profile. A module can see every login in that profile. Putting items in one profile means the user trusts them together. The web profiles dialog says so.
- **Old logins.** The folders `g-<groupId>-<moduleId>` and `shared-<moduleId>` are deleted at start. The user logs in again once per profile. The `app` profile for built-in panels is unchanged.
- **Old data.** A group's `profileOverrides` is no longer read. Like any unknown key, it is kept in the file.

This supersedes decisions 0005 and 0040.

## Alternatives

- **Keep decision 0040.** Module code stays apart, but logins repeat for every module and group.
- **One profile per group, without modules.** Fewer logins, but two groups cannot share one, and one group cannot keep two accounts of a site apart.
- **Profiles per device.** Logins are local anyway, but every device would need the same list built by hand, and synced items could not name a profile.
- **One file per profile in a `profiles/` folder.** It follows the other entities, but profiles change rarely and hold only a name, so the extra folder, watcher and conflict handling would add little.

## Consequences

- A site is logged in to once per profile and device, whatever the module.
- Module code in a profile can read the cookies of every site in it. The user separates what they do not trust together with profiles.
- Every user logs in again once after this change.
- A deleted profile's id can stay in synced groups and items. They use the fallback, and come back to the profile if it is restored, for example from a conflict copy.
- A profile deleted on another device leaves its logins on this device, as [decision 0039](0039-profiles-deleted-only-by-local-delete.md) accepts for groups.
