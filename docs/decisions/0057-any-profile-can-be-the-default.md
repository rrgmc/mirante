# 0057. Any profile can be the default

- Status: Accepted
- Date: 2026-10-29

## Context

Since [decision 0042](0042-user-managed-web-profiles.md), a profile called Default, with the id `default`, always exists. [Decision 0050](0050-profiles-hold-logins-connections-and-settings.md) kept that rule: Default cannot be renamed or deleted, its name is not stored, and it is listed in `workspace.mirante` only when it holds settings.

The user wants to rename Default, for example to "Work". What matters is that the workspace always has a profile to fall back on, not that this profile is a fixed one called Default.

## Decision

- **Every profile is ordinary.** Each one has a stored name, can be renamed, and can be deleted. The profile with the id `default` is no exception; its id stays, so its logins and tokens keep working.
- **One profile is marked as the default.** `workspace.mirante` names it with the key `defaultProfile`. The default profile is where the resolution order ends: an item uses its own profile, else its group's, else the default profile.
- **There is always at least one profile.** The last profile cannot be deleted. A workspace whose list is empty gets a profile called "Default", with the id `default`.
- **Deleting the default** marks another profile as the default: the first of the others by name, ignoring case.
- **A default that is not in the list,** for example because another device deleted it, is replaced in the same way when the file is read. Nothing is written until the next change.
- **Choosing the default.** The profiles dialog has **Set as Default** for the selected profile. Each profile's submenu in the Profiles menu has **Set as Default** too. Lists of profiles show the default first, with "(default)" after its name, then the others by name.
- **Groups.** The group dialog's first entry is "The default profile (<name>)". It is stored as no profile, as Default was before, so such a group follows the default when it changes. Choosing a profile by name stores its id.
- **Older files.** A `workspace.mirante` without `defaultProfile` was written before this decision. It gets a profile with the id `default`, named "Default" unless it is listed with a name, and that profile is the default. The file is written in the new form at the next change.

## Alternatives

- **Let only Default's name change.** Simpler, but Default still cannot be deleted, and the user would have one profile that behaves unlike the others.
- **Choose the default by position, such as the first in the list.** No new key, but reordering the list would change which profile items fall back on, without saying so.

## Consequences

- This supersedes the Default parts of decisions 0042 and 0050. The resolution order, the profile contents and storage stand.
- `Model` keeps the default's id, and `Model::defaultProfile()` always names a listed profile. `profiles::effective()` ends there instead of at `kDefaultProfile`. `kDefaultProfile` remains only as the id given to the first profile of a workspace.
- Deleting the profile with the id `default` deletes its logins on this device, like any other profile.
- Older versions of the app still treat `default` as Default: they show it as "Default", and fall back on it even when another profile is marked. Their writes keep `defaultProfile` as an unknown key, but drop the name of `default`, and drop its entry when it holds no settings. A workspace should be opened only with this version or later once a profile is renamed or the default is changed.
