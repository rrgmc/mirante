# Modules

A module is one folder of item types and the code they share. Modules are compiled into the app, and each one can be included or left out when building. This document is the contract between the core and the modules, and the guide for contributors. See [decision 0013](decisions/0013-compiled-in-modules.md).

## The modules

| Module | Item types | Connection kinds | Project settings |
|---|---|---|---|
| `gitlab` | `gitlab.mr.summary`, `gitlab.mr.page`, `gitlab.mr.list`, `gitlab.pipeline`, `gitlab.issue.summary`, `gitlab.issue.page`, `gitlab.issue.list` | `gitlab` | `project` |
| `jira` | `jira.issue.summary`, `jira.issue.page`, `jira.list` | `jira.cloud`, `jira.dc` | `projectKey` |
| `claude` | `claude.session.web`, `claude.session.local` | none | none |
| `web` | `web.page` | none | none |
| `notes` | `notes` | none | none |
| `folder` | `folder` | none | none |
| `image` | `image` | none | none |

All modules are enabled by default. The item types are described in [item types](item-types.md). Only the gitlab module has a [settings screen](#settings-screens) so far: General Configuration.

Built-in panels (`dash.panel`) belong to the core, not to a module, because the `dash://` scheme and its bridge are core code. See [web views](web-views.md#built-in-panels).

## Folder layout

```
src/modules/gitlab/
  CMakeLists.txt         calls mirante_add_module()
  README.md              what the module does, its types and connection kinds
  GitLabModule.cpp       the register function
  GitLabClient.h/.cpp    module code, freely organised
  icons/                 SVG icons, compiled in as resources
  ...
  tests/                 Qt Test sources for this module
```

A module may add more folders inside its own folder. It never adds files outside it.

## Ids

- A module id is lowercase letters and digits only, such as `gitlab`.
- Every id a module registers is the module id, or starts with the module id and a dot. This applies to item types (`gitlab.mr.summary`), connection kinds (`jira.cloud`) and settings.
- The prefix `dash` is reserved for the core. The build refuses a module with the id `dash`.
- An id has no empty parts: `gitlab..mr` and `gitlab.` are refused.

The prefix tells the core which module an unknown id belongs to, so a placeholder can name the missing module.

## Building

Each module's `CMakeLists.txt` calls one function, defined in `cmake/MiranteModule.cmake`:

```cmake
mirante_add_module(gitlab
    SOURCES GitLabModule.cpp GitLabClient.cpp ...
    TESTS   tests/tst_urls.cpp tests/tst_attention.cpp
    RESOURCES icons/gitlab.svg
    DEPENDS          # other module ids, usually none
    QT_LIBS          # further Qt components, usually none
    DEFAULT ON)
```

The function:

1. creates the option `MIRANTE_MODULE_GITLAB`, with the given default;
2. if the option is off, does nothing more;
3. builds a static library `mirante_module_gitlab`, linked only to the module interface and Qt: Core, Gui and Widgets, and the components in `QT_LIBS`, such as `WebEngineWidgets` for a module that builds its own web view from `profile()`;
4. compiles the resources in, served as `:/modules/<id>/<path>`, such as `:/modules/gitlab/icons/gitlab.svg`;
5. adds the module's tests to CTest, named `module_<id>_<file>`;
6. adds the module to the list of enabled modules.

The top-level `CMakeLists.txt` adds every folder in `src/modules/` that has a `CMakeLists.txt`. After all modules are added, it checks `DEPENDS`. If an enabled module depends on a disabled one, configuration stops with a message that names both.

**Choosing modules:**

- one option at a time: `-DMIRANTE_MODULE_JIRA=OFF`;
- an exact list: `-DMIRANTE_MODULES="gitlab;web;notes"`, which enables those modules and disables all others;
- a configure preset in `CMakePresets.json`, for builds that are used often.

**Registration.** CMake generates `enabled_modules.cpp` in the build folder. It calls each enabled module's register function, in alphabetical order of module id:

```cpp
void registerEnabledModules(ItemTypeRegistry &registry)
{
    registry.registerModule(QStringLiteral("folder"), &mirante::modules::folder::registerModule);
    registry.registerModule(QStringLiteral("gitlab"), &mirante::modules::gitlab::registerModule);
    // ...
}
```

`main.cpp` calls `registerEnabledModules()` once at startup. The generated file also lists the enabled module ids, which the About dialog shows.

Modules do not register themselves through static objects. The linker drops unreferenced objects from static libraries, so such registration fails silently on some platforms.

## The module interface

A module links to one CMake target, `mirante::module_api`. It holds the only core headers a module may include. A module that includes any other core header does not compile, because the include path is not available to it.

The module interface contains the headers below. The "Since" column gives the milestone that adds each header, or "planned" where the [roadmap](roadmap.md) does not name one yet.

| Header | Contents | Since |
|---|---|---|
| `Module.h` | `ModuleRegistrar`, the object a module registers its parts with. `setModuleName()`, `addSettingsScreen()` and `settings()` were added after M4; see [settings screens](#settings-screens). | M2 |
| `ModuleSettings.h` | `SettingsScreen`, one configuration screen of a module, and `ModuleSettings`, which reads the values of the module's screens. See [settings screens](#settings-screens). | after M4 |
| `ItemType.h` | The `ItemType` interface. See [item types](item-types.md#the-type-registry). `shortName()`, the short label in cell headers, was added after M3; see [decision 0033](decisions/0033-cell-header-pills.md). M4 adds `rendering()`, `connectionKinds()` and `createView()`. | M2 |
| `Fields.h` | `Field`: a key, a label, a type, a default value, a required flag, and for a choice its values and labels. The types are text, multi-line text, URL, integer, boolean, choice, folder, image and text list. A text field can offer `suggestions` in an editable drop-down list. Used for item configuration and settings screens, and from M5 for connection fields and project settings. See decisions [0028](decisions/0028-item-fields-in-m3.md) and [0044](decisions/0044-module-settings-in-the-workspace.md). | M3 |
| `ItemEditor.h` | `ItemEditor`, the base of an item type's own configuration editor, and `EditorContext`, what an editor can reach: the project's folders and a way to save an image into `files/` | M3 |
| `ItemContext.h` | `ItemContext`, everything an item's code can reach. See [below](#item-context). | M4 |
| `ConnectionKind.h` | The `ConnectionKind` interface | M5 |
| `ApiClient.h` | The shared HTTP client base: ETags, paging, rate limits, request limits | M5 |
| `Status.h` | Status results and attention reasons for the [status cache](integrations.md#polling-and-caching) | M5 |
| `QuickAdd.h` | Quick add handlers and related-item finders | M7 |
| `Values.h` | Read-only views of a group, project, task, item and folder | planned |

Modules see entities only through read-only views. Changes go through actions on `ItemContext`, which call the core's `Model` methods.

## What a module registers

The register function receives a `ModuleRegistrar` and adds any of the following:

| Part | Description |
|---|---|
| Item types | One object per type, with the properties listed in [item types](item-types.md#the-type-registry). For each type the module chooses how its configuration is edited: it lists its fields, and the core builds the form, or it returns an editor of its own. See [decision 0028](decisions/0028-item-fields-in-m3.md). |
| Module name | The name the user sees, such as "GitLab", with `setModuleName()`. The module id is used without one. |
| Settings screens | Configuration screens of the whole module, such as GitLab's "General Configuration", with values kept in the workspace. See [settings screens](#settings-screens). |
| Connection kinds | Fields of the connection, how to sign a request, how to test the connection |
| Project settings | Fields shown in the project dialog, such as the GitLab project path. They are defaults for the module's items. |
| Device settings | Fields kept on each device, such as an editor command |
| Quick add handlers | Recognise pasted text and propose a task. See [quick add](#quick-add). |
| Related-item finders | Find items related to a piece of text, such as merge requests whose branch contains a Jira key |

The core builds every dialog from the field definitions, and checks the values against them. It stores and edits the values without knowing what they mean. The one exception is an item type with its own editor: the item dialog shows that editor instead of a form. Either way, keys of the configuration that the form or editor does not show are kept unchanged.

## Settings screens

A module can add configuration screens for values that belong to the whole module, not to one item or project. For example, the gitlab module keeps a list of project paths, and offers them wherever an item asks for a project. See [decision 0044](decisions/0044-module-settings-in-the-workspace.md).

```cpp
class GeneralScreen : public mirante::SettingsScreen
{
public:
    QString id() const override { return QStringLiteral("general"); }
    QString name() const override { return tr("General Configuration"); }
    std::vector<Field> fields() const override;   // or createEditor()
};

void registerModule(ModuleRegistrar &registrar)
{
    registrar.setModuleName(QStringLiteral("GitLab"));
    registrar.addSettingsScreen(std::make_unique<GeneralScreen>());
    const ModuleSettings &settings = registrar.settings();
    // Item types may keep &settings and call settings.values("general").
}
```

- **Id and name.** The id names the stored values, so it never changes. It has lowercase letters and digits only, and is unique within the module. The name is the menu entry and the dialog title.
- **Fields or an editor.** As for item types, a screen lists its `fields()`, and the core builds the form, or returns its own `ItemEditor` from `createEditor()`. The editor gets the stored object with `setConfig()`, and its `config()` is stored. Keys that the form or editor does not show are kept.
- **Values.** Each screen's values are one JSON object, in `workspace.mirante` under `moduleSettings.<module id>.<screen id>`. They sync with the workspace. See [data model](data-model.md#module-settings).
- **No secrets.** A screen never asks for a token or a password. Those stay in the keychain, as for [connections](#connections-and-tokens).
- **Reading.** `registrar.settings()` returns a `ModuleSettings`, which lives as long as the module's item types. `values(screenId)` reads the current values of one of the module's screens on each call, so code reads them where it needs them: in `fields()`, an editor or a view. It returns an empty object when nothing is stored.
- **Menu.** The core adds a submenu named after the module to the [Modules menu](ui.md#modules), with one entry per screen. A module without screens does not appear there.

## Item context

Each item's code gets an `ItemContext`. It gives access to the configuration the core owns, without the core's classes. The core makes one for each placement, and it outlives the placement's view. The "Since" column gives the milestone that adds each member.

| Member | Description | Since |
|---|---|---|
| `itemName()`, `config()` | The item's name and configuration, read from the model on each call | M4 |
| `item()`, `task()`, `project()`, `group()` | Read-only views of the item and where it sits | planned |
| `projectSetting(key)` | A project setting the module declared, such as `project` for GitLab. Undefined if the project does not set it. | M4 |
| `deviceSetting(key)` | A device setting the module declared | M7 |
| `connection(kinds)` | The first connection of one of the kinds found by the [resolution order](data-model.md#connection). In M4 it gives the details without a token; from M5 the handle also signs requests, and reports a missing token or a failure. See [decision 0038](decisions/0038-read-connections-in-m4.md). | M4 |
| `status()` | This item's entry in the status cache: fetch, read cached data, report attention | M5 |
| `createWebView(url)` | A live web page in a plain `QWidget`, in the item's [web profile](web-views.md#profiles), with popups, permissions, downloads and the toolbar handled by the core. The normal way for a web type to show a page. See [decision 0037](decisions/0037-core-makes-web-views.md). | M4 |
| `profile()` | The item's [web profile](web-views.md#profiles), for a module that builds its own `QWebEngineView`. Such a module links Qt WebEngine with `QT_LIBS`, and handles popups, permissions and downloads itself. The profile holds the logins of every site that its items visit, including those of other modules; see [decision 0042](decisions/0042-user-managed-web-profiles.md). | M4 |
| `createMessageView(text)` | A grey message in the cell, such as "No GitLab connection in this group" | M4 |
| `folderPath(folderId)` | The local path of a project folder on this device, or empty if not mapped | M4 |
| `gitHead(path)` | The current branch of a folder | M7 |
| `launchTerminal(dir, command)` | Starts a command in a terminal. See [terminal launch](integrations.md#terminal-launch). | M7 |
| `updateConfig(config)` | Changes the item's configuration through `Model` | M4 |

### Connections and tokens

A connection handle gives a module:

- **`details()`:** the kind, the base URL and the kind's own fields, such as the email of a Jira Cloud connection.
- **`authorize(request)`:** adds the authentication to a `QNetworkRequest`. The connection kind decides how, for example with a `PRIVATE-TOKEN` header for GitLab. Item code never sees the token. This is the normal way to call an API.
- **`secret()`:** returns the raw token, asynchronously. Use it only when a request is not enough, such as passing a token to a command in a terminal. Every call needs a comment that says why `authorize()` does not work there.

Tokens are loaded from the keychain before the first request of a connection. If no token is stored, `authorize()` fails and the core shows "Token not set on this device", as described in [errors](integrations.md#errors).

The connection kind's own code is the only module code that receives a token for signing.

## Quick add

Quick add is generic. The core passes the pasted text to every quick add handler. Each handler can:

- recognise the text, such as a merge request URL or a Jira key;
- propose a task title and its items;
- name a project setting and value to match, such as `gitlab.project = acme/backend`. The core picks the project whose setting has that value. If none matches, or several do, it asks the user.

The core then passes the new task's title and its items' text, such as a branch name, to every related-item finder. A finder can offer more items. For example, the `gitlab` module finds merge requests whose branch contains a Jira key, and the `jira` module recognises a Jira key in a merge request title.

This lets the `jira` and `gitlab` modules work together without depending on each other. See [ui](ui.md#quick-add).

## Attention

Item types report attention reasons through `status()`. A reason has a text, such as "Pipeline failed", and two flags:

- `notify`: also send a [desktop notification](ui.md#tray-and-notifications);
- `closed`: the item's work is finished, such as a merged merge request or a closed Jira issue.

The core suggests marking a task done when every item whose type reports `closed` reports it as true. See [data model](data-model.md#task).

## Missing modules

Synced data can reach a build that does not include a module. For example, one device builds without `jira`. The core then:

- **Items:** shows a placeholder: "This item needs the jira module, which this build does not include." The item's `type` and `config` are kept unchanged.
- **Connections:** lists the connection read-only in the group dialog, with the same message.
- **Project settings:** keeps the module's settings unchanged and does not show them.
- **Module settings:** keeps the values of the module's settings screens unchanged in `workspace.mirante`. The Modules menu has no entry for the module.
- **Cells and Focus:** show the item's name. No status or attention is computed for it.

Everything is written back exactly as it was loaded. See [sync](sync.md#the-data-folder).

## Rules for module code

- Use only the module interface and Qt. Do not include core headers.
- Use another module's headers only if that module is listed in `DEPENDS`. Prefer quick add handlers and related-item finders, which need no dependency.
- Platform-specific code belongs in the core's `platform/`. If a module needs something new from the platform, add it to `ItemContext` first.
- Everything runs on the main thread. See [threading](architecture.md#threading).
- Each module tests at least its URL recognition and its attention rules, on recorded API responses.
- Each module's `README.md` lists its item types, connection kinds, settings and settings screens.

## Adding a module

1. Create `src/modules/<id>/` with a `CMakeLists.txt` that calls `mirante_add_module()`.
2. Write `registerModule(ModuleRegistrar &)` in the namespace `mirante::modules::<id>`.
3. Give the module a name with `setModuleName()`. Register the item types and any settings screens, connection kinds, settings and quick add handlers. Give each item type a short label with `shortName()`, such as "MR", for its cell headers.
4. Add tests and a `README.md`.
5. Add the module's item types to [item types](item-types.md) and the module to the table at the top of this document.
6. Build once with the module on and once with it off. Both must build and pass the tests.
