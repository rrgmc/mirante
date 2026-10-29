# 0054. Module settings replace connections

- Status: Accepted
- Date: 2026-10-29

## Context

[Decision 0050](0050-profiles-hold-logins-connections-and-settings.md) moved connections from groups into profiles. A profile then had a list of connections, each with a kind, a name and a base URL, a default for each kind, and its own editor in the profiles dialog. It also had module settings, edited from the Modules menu.

GitLab and Jira are the only connection kinds. In practice a profile talks to one GitLab server and one Jira site. Two places to set up one server, connections and module settings, were one too many. The user asked for the server to be a module setting of the profile, next to the module's other settings.

## Decision

- **One server per module and profile.** GitLab's General Configuration gains **Base URL**, `https://gitlab.com` by default. Jira gets a General Configuration screen with **Base URL**, the site, such as `https://acme.atlassian.net`, without a default. The values live in the profile's module settings: `gitlab.general.baseUrl` and `jira.general.baseUrl`.
- **The token on the same screen.** Each of these screens has a **Token** field. A new field type, `Field::Type::Secret`, shows it hidden. Its value is never part of the stored values, so it never reaches `workspace.mirante`. The core hands it to a secret store under the keychain key `<profile id>/<module id>/<field key>`, such as `default/gitlab/token`, with the service `mirante`. No code stores tokens yet: qtkeychain is used from M5. Until then the core logs that a secret was entered, without its value, and drops it. M5 fills `EntityActions::setSaveSecret()` with the keychain.
- **Connections go.** The connection list, its dialog and editor, the Connections… button in the profiles dialog, `connections::resolve`, `ItemContext::connection()`, `ConnectionDetails`, `ItemType::connectionKinds()`, `ItemTypeRegistry::connectionKinds()`, `Item::connectionId` and `Project::connectionOverrides` are removed. Their keys in files, such as `connectionId` in an item, are kept as unknown keys.
- **Modules read the server from their settings.** A view reads it with `ItemContext::moduleSettings(screenId)`, in the item's profile.
- **Pasted links.** `ItemType::fromUrl(url, profileId)` takes the profile the new item would use instead of a list of base URLs. A module reads its server there. The core no longer names a connection for an item made from a link ([decision 0049](0049-items-from-a-pasted-link.md)).
- **Which types use a profile.** A type uses one when its views are web views or its module has settings screens. GitLab and Jira types use one through their screens.
- **Migration.** When a workspace opens, connections become base URLs, in `src/core/ProfileMigration.cpp`. Connections kept in a profile by decision 0050 are taken first, then those of groups from before it, by group order. A connection of kind `gitlab` gives `gitlab.general.baseUrl`; one of kind `jira.cloud` or `jira.dc` gives `jira.general.baseUrl`. The default connection of the kind wins, Jira Cloud before Data Center, else the first one. A base URL the profile already has wins over all. Each connection that is not used is dropped with a line in the log. Connections of other kinds, which no module of this version reads, are kept in the profile under the old keys. Running the migration again changes nothing. The files are written once, as in decision 0050.

This supersedes [decision 0006](0006-connections-per-group.md), and the connection parts of decisions [0038](0038-read-connections-in-m4.md), [0049](0049-items-from-a-pasted-link.md) and [0050](0050-profiles-hold-logins-connections-and-settings.md).

## Alternatives

- **Keep connections, and add the base URL to module settings too.** Two places for one server.
- **Several servers per module and profile.** A client's own Jira next to the company's would fit in one profile. But the user works with one server per module in a context; a second one gets its own profile.
- **Migrate tokens.** None were ever stored, so there is nothing to move.

## Consequences

- A profile is set up in one place per module: the server, its token and the module's other settings.
- Jira Cloud and Data Center are no longer told apart by a connection kind. M5 must tell them apart another way, such as from the site's address or a setting on the Jira screen, and may need an email field for Jira Cloud.
- Items made from a pasted link no longer remember a server of their own. They follow their profile.
- Status data, cached by connection in the design of M5, is cached by profile and module instead.
