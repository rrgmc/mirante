# 0044. Modules have settings screens, stored in the workspace

- Status: Accepted
- Date: 2026-10-29

## Context

Some values belong to a module as a whole, not to one item or project. For example, the user works on a handful of GitLab projects and types their paths again for every new item. The user wants each module to offer one or more configuration screens, such as GitLab's "General Configuration", reached from a menu. The module names its screens.

The values should follow the user to every device, like groups and web profiles. Secrets, such as tokens, must not be synced.

## Decision

- **Screens.** A module adds zero or more settings screens with `ModuleRegistrar::addSettingsScreen()`. A `SettingsScreen` has a stable id, such as `general`, and a name for the menu, such as "General Configuration". Like an item type ([decision 0028](0028-item-fields-in-m3.md)), it lists `fields()`, and the core builds the form, or returns its own `ItemEditor` from `createEditor()`. Screen ids are lowercase letters and digits, unique within the module.
- **Module name.** `ModuleRegistrar::setModuleName()` gives the module a name for the user, such as "GitLab". Without one, the module id is used.
- **A list field.** `Field` gains the type `TextList`, a JSON array of one-line texts, edited one per line. It also gains `suggestions`, values that a text field offers in an editable drop-down list.
- **Storage.** The values live in `workspace.mirante`, under `moduleSettings`, as one JSON object per screen: `moduleSettings.<module id>.<screen id>`. They sync with the workspace, like the web profile list of [decision 0042](0042-user-managed-web-profiles.md). Keys that a form or editor does not show are kept. So are the settings of modules and screens that this build does not have. A screen with no values has no entry.
- **No secrets.** Tokens and other secrets stay in the keychain, as for connections. A screen must not ask for one.
- **Reading.** `ModuleRegistrar::settings()` gives each module a `ModuleSettings`. Its `values(screenId)` reads the current values of one of the module's screens. It lives as long as the module's item types, so they can keep it and read it in `fields()`, in an editor or in a view.
- **Menu.** A Modules menu, between View and Help, has a submenu for each module with screens, named after the module. The submenu lists the screens by their names. Each entry opens a dialog for that screen. Modules without screens do not appear, and without any such module the menu is left out.
- **First user.** The gitlab module adds "General Configuration", with a list of project paths. Every GitLab "project" field offers these paths.

## Alternatives

- **One object per module instead of per screen.** Keys of two screens could clash, and an editor of its own would get keys it does not own. One object per screen keeps each screen's keys apart.
- **A file per module, such as `modules/gitlab.json`.** Two devices that change different modules at once would not conflict. But settings change rarely, and a new folder needs watching, reloading and conflict handling, as decision 0042 found for web profiles.
- **Device settings.** No sync, so each device would need the same list typed again.
- **Read through `ItemContext`.** An `ItemContext` exists only for a placement's view. The item dialog builds its form from `fields()` without one, and that is where GitLab offers its paths. The registrar reaches every place a module's code runs.
- **A Settings or Preferences dialog with a page per module.** One dialog would be tidy, but the user asked for one menu entry per screen with the module's own names.

## Consequences

- A module can keep shared values without the core knowing what they mean.
- `workspace.mirante` changes more often. Two devices that change it at the same time make a conflict copy, which the user resolves as for any other file.
- A field's suggestions are read each time `fields()` is called, so a change shows the next time a form opens.
- The Modules menu grows with every module that adds a screen.
