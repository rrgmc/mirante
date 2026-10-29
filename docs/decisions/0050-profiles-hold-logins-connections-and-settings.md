# 0050. Profiles hold logins, connections and module settings

- Status: Accepted
- Date: 2026-10-29

## Context

[Decision 0042](0042-user-managed-web-profiles.md) made web profiles a list the user manages, such as "Work" and "Personal". Groups choose a default, and items may choose their own. Two other things that differ by account were kept elsewhere:

- Connections, with their tokens, belonged to groups ([decision 0006](0006-connections-per-group.md)). Two groups that work for the same client needed the same connections twice.
- Module settings, such as GitLab's list of project paths, belonged to the whole workspace ([decision 0044](0044-module-settings-in-the-workspace.md)). The paths of work projects were offered in personal items too.

The user asked for profiles to be general data profiles. A profile should group everything that belongs to one account or context: web logins, API connections and tokens, and the modules' settings.

## Decision

- **A profile holds three things:** its web logins, as in decision 0042; its connections, each with its token; and the values of the modules' settings screens.
- **One name.** The concept is called "profile" in the user interface, the code and the documents. File > Profiles… replaces File > Web Profiles…. Glossary: [Profile](../README.md#glossary).
- **Resolution stays.** A group chooses a default profile, and an item may choose its own. An item uses its own profile, else its group's, else Default. A step that names a missing profile is skipped. Default always exists. It cannot be renamed or deleted, but its web data can be cleared.
- **Connections move from groups to profiles.** A connection keeps its fields: kind, name, base URL and the kind's own fields. A profile has a default connection for each kind. An item's `connectionId` and a project's override name a connection of the item's profile. The resolution order is the item's choice, the project's override, then the profile default. The group dialog loses its Connections box. The profiles dialog opens the same connection editor for the selected profile, with Connections….
- **Pasted links.** A link pasted to make an item ([decision 0049](0049-items-from-a-pasted-link.md)) is read with the connections of the group's profile, the one a new item uses. If the user chooses another profile for the item, the item takes that profile's connection of the same kind and base URL, or none.
- **Module settings move to profiles.** Each profile keeps the values of each screen. The Modules menu stays as decision 0044 describes it. Its dialog gets a profile list at the top, with Default chosen first. Values edited for several profiles are stored together on OK.
- **Modules read the item's profile.** `ModuleSettings::values()` takes a profile id. `ItemContext::moduleSettings(screenId)` reads the screen's values in the item's profile, for a view. `ItemType::fieldsForProfile(profileId)` gives the fields for an item that uses that profile. The item dialog calls it with the profile the edited item would use, and builds the form again when another profile is chosen. `fields()` stays for everything else, such as validation.
- **The item dialog offers a profile to more types.** A type uses a profile when its views are web views, it lists connection kinds, or its module has settings screens. The dialog offers a profile for these types only.
- **Storage.** The profiles stay in `workspace.mirante`, under the new key `profiles`. Each entry holds its id, its name, its `connections` without tokens, its `defaultConnections` and its `moduleSettings`, and keeps unknown keys. Default is listed only when it holds something, with the id `default` and no name. Groups and items name their profile with the key `profile`. The old key `webProfile` is still read.
- **Tokens.** Tokens stay in the operating system keychain, keyed by profile id and connection id: `<profileId>/<connectionId>`.
- **Migration.** When a workspace is opened, data from before this decision moves into profiles:
  - the list under `webProfiles` becomes `profiles`;
  - module settings at the top of `workspace.mirante` go to Default;
  - each group's connections go to the profile that group uses, or to Default. They keep their ids, so items and project overrides still find them. Groups are taken by their order. A name the profile already uses gets the group's name added, such as "gitlab.com (Personal)". A group default becomes the profile default if the profile has none of that kind. Otherwise the group's projects without an override for that kind get the group default as their override, so their items keep the same connection.

  The app writes `workspace.mirante` at once, then the changed group and project files. A connection whose id a profile already has is skipped, so running the migration again changes nothing. A group file with old connections that arrives later, for example from a device with an older version, is migrated in the same way. The migration does not run while `workspace.mirante` was written by a newer version.
- **Keychain migration.** None is needed. Before this decision no version stored a token: tokens, and qtkeychain calls, come in M5. The connection ids do not change, so M5 starts with the new keys.
- **Warning.** The profiles dialog says that items in the same profile share its logins, connections and tokens.

This supersedes decision 0042 in part: the list's key, the dialog's name, and what a profile holds. It supersedes decision 0044 in part: where the values are stored, how modules read them, and the profile list in the dialog. It supersedes decision 0006 in part: connections, their defaults and their tokens belong to profiles instead of groups. It changes decision 0049 in part: a pasted link is read with the connections of a profile.

## Alternatives

- **Module settings as a section per module in the profiles dialog.** All of a profile would be in one place. But the dialog would grow with every module, and the user asked for one menu entry per screen, with the module's names. A profile list in the existing dialog keeps both: the screen the user knows, and a clear choice of whose values are shown.
- **Keep connections in groups, and only add a profile per connection.** Two groups for one client would still repeat connections.
- **One file per profile, such as `profiles/<id>.json`.** Two devices that change different profiles would not conflict. But profiles change rarely, and a new folder needs watching, reloading and conflict handling, as decision 0042 found.
- **Keep the stored keys `webProfiles` and `webProfile`.** No migration of names would be needed. But the files would keep a name the app no longer uses, which misleads anyone who reads them.
- **Migrate in memory only, and write the files when they next change.** Fewer writes. But a group file with old connections would bring a deleted connection back on the next start.

## Consequences

- A context such as a client is set up once, as a profile, and every group or item can use it.
- Items in one profile share logins, connections, tokens and settings. Separating them means using two profiles.
- Older versions of the app read neither `profiles` nor `profile`. On such a device every item uses Default, without connections. Their writes keep the new keys as unknown keys.
- `workspace.mirante` holds more and changes more often. Two devices that change it at the same time make a conflict copy, as before.
- The item dialog of a GitLab item offers the project paths of the item's profile.
