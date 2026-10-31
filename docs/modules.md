# Modules

A module is one folder of item types and the code they share. Modules are compiled into the app, and each one can be included or left out when building. This document is the contract between the core and the modules, and the guide for contributors. See [decision 0013](decisions/0013-compiled-in-modules.md).

## The modules

| Module | Item types | Settings screens | Project settings |
|---|---|---|---|
| `gitlab` | `gitlab.mr.summary`, `gitlab.mr.page`, `gitlab.mr.list`, `gitlab.pipeline`, `gitlab.pipeline.list`, `gitlab.issue.summary`, `gitlab.issue.page`, `gitlab.issue.list` | General Configuration: base URL, token, project paths | `project` |
| `github` | `github.pr.summary`, `github.pr.page`, `github.pr.list`, `github.run`, `github.run.list`, `github.issue.summary`, `github.issue.page`, `github.issue.list` | General Configuration: base URL, token, repositories | `repository` |
| `jira` | `jira.issue.summary`, `jira.issue.page`, `jira.list` | General Configuration: base URL, token | `projectKey` |
| `claude` | `claude.session.web` | none | none |
| `web` | `web.page` | none | none |
| `notes` | `notes` | none | none |
| `folder` | `folder` | none | none |
| `image` | `image` | none | none |

All modules are enabled by default. The item types are described in [item types](item-types.md). The gitlab, github and jira modules keep the server they talk to in their [settings screens](#settings-screens), one per profile; see [decision 0054](decisions/0054-module-settings-replace-connections.md).

Built-in panels (`dash.panel`) belong to the core, not to a module in `src/modules/`, because the `dash://` scheme and its bridge are core code. The core registers them as its own module, `dash`, named "Built in", with `ItemTypeRegistry::registerBuiltInModule()` before the enabled modules. It is part of every build, and the item dialog offers its types no profile, since panels use the app profile. See [web views](web-views.md#built-in-panels) and [decision 0094](decisions/0094-built-in-panels-as-a-core-item-type.md).

## Folder layout

```
src/modules/gitlab/
  CMakeLists.txt         calls mirante_add_module()
  README.md              what the module does, its types and settings
  GitLabModule.cpp       the register function
  GitLabClient.h/.cpp    module code, freely organised
  icons/                 SVG icons, compiled in as resources
  ...
  tests/                 Qt Test sources for this module
```

A module may add more folders inside its own folder. It never adds files outside it.

## Ids

- A module id is lowercase letters and digits only, such as `gitlab`.
- Every id a module registers is the module id, or starts with the module id and a dot. This applies to item types (`gitlab.mr.summary`) and settings.
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
| `Module.h` | `ModuleRegistrar`, the object a module registers its parts with. `setModuleName()`, `addSettingsScreen()` and `settings()` were added after M4; see [settings screens](#settings-screens). So was `setModuleDescription()`; see [decision 0056](decisions/0056-modules-window.md). M5 adds `setServerType()` and `addProjectSetting()`. M7 adds `addQuickAddHandler()`, `addRelatedItemFinder()` and `addDeviceSetting()`. `setModuleUuid()`, `setModuleVersion()`, `setModuleAuthor()` and `setModuleHomepage()` were added after M7; see [decision 0089](decisions/0089-module-identity.md). | M2 |
| `ModuleSettings.h` | `SettingsScreen`, one configuration screen of a module, and `ModuleSettings`, which reads the values of the module's screens in a profile. See [settings screens](#settings-screens). | after M4 |
| `ItemType.h` | The `ItemType` interface. See [item types](item-types.md#the-type-registry). `shortName()`, the short label in cell headers, was added after M3; see [decision 0033](decisions/0033-cell-header-pills.md). M4 adds `rendering()` and `createView()`; its `connectionKinds()` was removed by [decision 0054](decisions/0054-module-settings-replace-connections.md). `fromUrl()` was added after M4; see [items from a link](#items-from-a-link). So was `fieldsForProfile()`; see [decision 0050](decisions/0050-profiles-hold-logins-connections-and-settings.md). `description()` was added after M4; see [decision 0056](decisions/0056-modules-window.md). M5 adds `hasStatus()` and `fetchStatus()`; see [status](#status). M7 adds `viewKeys()`; see [decision 0081](decisions/0081-views-that-keep-their-own-keys.md). | M2 |
| `UrlMatch.h` | `UrlMatch`, what an item type makes of a pasted web address, and `pathBelow()`, which reads an address below a server's base URL. See [items from a link](#items-from-a-link). | after M4 |
| `Fields.h` | `Field`: a key, a label, a type, a default value, a required flag, and for a choice its values and labels. The types are text, multi-line text, URL, integer, boolean, choice, folder, image, text list and secret. A text field can offer `suggestions` in an editable drop-down list. A secret, such as a token, is for settings screens only; its value is never stored with the others. Used for item configuration and settings screens, and from M5 for project settings. See decisions [0028](decisions/0028-item-fields-in-m3.md), [0044](decisions/0044-module-settings-in-the-workspace.md) and [0054](decisions/0054-module-settings-replace-connections.md). | M3 |
| `ItemEditor.h` | `ItemEditor`, the base of an item type's own configuration editor, and `EditorContext`, what an editor can reach: the project's folders and a way to save an image into `files/` | M3 |
| `ItemContext.h` | `ItemContext`, everything an item's code can reach. See [below](#item-context). | M4 |
| `ApiClient.h` | `ServerType`, how a module's server is set up and authenticated, and `ApiClient`, a module's server in one profile: requests with ETags, paging, rate limits and request limits. See [servers and tokens](#servers-and-tokens). | M5 |
| `Status.h` | `ItemStatus`, `AttentionReason`, `StatusJob` and `ItemStatusSource`, for the [status cache](integrations.md#polling-and-caching). See [status](#status). M7 adds `StatusJob::folderPath()` and `gitHead()`. | M5 |
| `widgets/*.h` | The [view kit](#the-view-kit): small widgets that native views share. | M6 |
| `QuickAdd.h` | Quick add handlers and related-item finders. See [quick add](#quick-add). | M7 |
| `GitState.h` | `GitState`, a folder's Git state, as `ItemContext::gitStatus()` reports it | M7 |
| `Values.h` | Read-only views of a group, project, task, item and folder | planned |

Modules see entities only through read-only views. Changes go through actions on `ItemContext`, which call the core's `Model` methods.

## What a module registers

The register function receives a `ModuleRegistrar` and adds any of the following:

| Part | Description |
|---|---|
| Item types | One object per type, with the properties listed in [item types](item-types.md#the-type-registry). For each type the module chooses how its configuration is edited: it lists its fields, and the core builds the form, or it returns an editor of its own. See [decision 0028](decisions/0028-item-fields-in-m3.md). |
| Module name | The name the user sees, such as "GitLab", with `setModuleName()`. The module id is used without one. |
| Module description | One or two plain sentences that say what the module adds, with `setModuleDescription()`, such as "Merge requests, pipelines and issues from a GitLab server." The [Modules window](ui.md#modules-window) shows it under the module's name. |
| Module identity | A UUID with `setModuleUuid()`, a version such as `1.2.0` with `setModuleVersion()`, an author with `setModuleAuthor()` and an `http` or `https` homepage with `setModuleHomepage()`. The UUID never changes and no two modules share one; a new module takes a newly generated one. The Modules window shows all four. See [decision 0089](decisions/0089-module-identity.md). |
| Settings screens | Configuration screens of the whole module, such as GitLab's "General Configuration", with values kept in each profile of the workspace. See [settings screens](#settings-screens). |
| Project settings | Fields shown in the project dialog, such as the GitLab project path, with `addProjectSetting()`. They are defaults for the module's items. |
| Server | How the module's server is set up and authenticated, with `setServerType()`. See [servers and tokens](#servers-and-tokens). |
| Device settings | Fields kept on each device, such as an editor command, with `addDeviceSetting()`. Types Text, Integer, Bool and Choice. The [settings window](ui.md#settings) edits them, and items read them with `ItemContext::deviceSetting()`. See [decision 0079](decisions/0079-module-device-settings.md). |
| Quick add handlers | Recognise pasted text and propose a task, with `addQuickAddHandler()`. See [quick add](#quick-add). |
| Related-item finders | Find items related to a piece of text, such as merge requests whose branch contains a Jira key, with `addRelatedItemFinder()` |

The core builds every dialog from the field definitions, and checks the values against them. It stores and edits the values without knowing what they mean. The one exception is an item type with its own editor: the item dialog shows that editor instead of a form. Either way, keys of the configuration that the form or editor does not show are kept unchanged.

## Settings screens

A module can add configuration screens for values that belong to the whole module, not to one item or project. Each [profile](data-model.md#profile) has its own values, and an item uses those of its profile. For example, the gitlab module keeps a list of project paths, and offers the paths of the item's profile wherever an item asks for a project. See decisions [0044](decisions/0044-module-settings-in-the-workspace.md) and [0050](decisions/0050-profiles-hold-logins-connections-and-settings.md).

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
    // Item types may keep &settings and call settings.values(profileId, "general")
    // in fieldsForProfile(profileId).
}
```

- **Id and name.** The id names the stored values, so it never changes. It has lowercase letters and digits only, and is unique within the module. The name is the menu entry and the dialog title.
- **Fields or an editor.** As for item types, a screen lists its `fields()`, and the core builds the form, or returns its own `ItemEditor` from `createEditor()`. The editor gets the stored object with `setConfig()`, and its `config()` is stored. Keys that the form or editor does not show are kept.
- **Values.** Each screen's values are one JSON object per profile, in that profile's entry of `workspace.mirante`, under `moduleSettings.<module id>.<screen id>`. They sync with the workspace. See [data model](data-model.md#module-settings).
- **Secrets.** A token or a password goes in a field of type `Secret`. Its value is never part of the stored values, so it never reaches the workspace. The core keeps it in the keychain, under `<workspace id>/<profile id>/<module id>/<field key>`; see [servers and tokens](#servers-and-tokens).
- **Reading.** Module code reads the values of the profile an item uses. There are three ways:
  - in a view, `ItemContext::moduleSettings(screenId)` reads them for the item's profile;
  - in a form, the item dialog calls the type's `fieldsForProfile(profileId)` with the profile the edited item would use, and the type reads them there, for example to offer suggestions. `fields()` stays the plain list, used where no profile is known, such as validation;
  - anywhere else, `registrar.settings()` returns a `ModuleSettings`, which lives as long as the module's item types. Its `values(profileId, screenId)` reads one screen's values in a profile.

  Each call reads the current values, so code reads them where it needs them. An empty object means nothing is stored.
- **Menu.** The core adds a submenu named after the module to each profile's Modules submenu in the [Profiles menu](ui.md#profiles-menu), with one entry per screen. Its dialog edits that profile's values. A module without screens does not appear there.

## Items from a link

An item type can read a web address that the user pasted, such as a merge request's page, and return the configuration that shows it. The new item dialog then offers only the types that take the address, and fills the chosen one. See [decision 0049](decisions/0049-items-from-a-pasted-link.md) and [the item dialog](ui.md#item-dialog).

```cpp
std::optional<UrlMatch> fromUrl(const QUrl &url, const QString &profileId) const override
{
    // The server in this module's settings, in the profile of the new item.
    const QUrl base(m_settings.values(profileId, "general").value("baseUrl").toString());
    if (const std::optional<QString> path = pathBelow(url, base)) {
        // Read *path, such as "group/project/-/merge_requests/42".
    }
    return std::nullopt;     // not an address of this type
}
```

- **The address.** `url` is an `http` or `https` address with a host. The default `fromUrl()` takes none.
- **The profile.** `profileId` is the profile the new item would use: its group's. The type reads its server there, from its module settings, so it can read a self-hosted server, including one installed at a path. `pathBelow(url, base)` returns the path of `url` below `base`, or nothing if `url` is on another server.
- **The match.** `config` holds the keys of the type's own fields; keys left out take the fields' defaults. Map the address only to fields the type already has. `name` is a suggested item name, such as `acme/backend!42`. `baseUrl` is the base URL of the server the address belongs to, for information; the core does not use it. `generic` marks a type that takes any address, such as Web page; the dialog lists such types last.
- **Tests.** Test `fromUrl()` with good addresses and with addresses it must refuse.

The types that take addresses so far are listed in [item types](item-types.md#items-from-a-link).

## Item context

Each item's code gets an `ItemContext`. It gives access to the configuration the core owns, without the core's classes. The core makes one for each placement, and it outlives the placement's view. The "Since" column gives the milestone that adds each member.

| Member | Description | Since |
|---|---|---|
| `itemName()`, `config()` | The item's name and configuration, read from the model on each call | M4 |
| `item()`, `task()`, `project()`, `group()` | Read-only views of the item and where it sits | planned |
| `projectSetting(key)` | A project setting the module declared, such as `project` for GitLab. Undefined if the project does not set it. | M4 |
| `deviceSetting(key)` | A device setting the module declared, or its field's default | M7 |
| `filePath(fileName)` | The absolute path of a file in the data folder's `files/`, such as an image | M7 |
| `moduleSettings(screenId)` | The values of one of the module's [settings screens](#settings-screens), in the item's profile. Empty if nothing is stored. A module reads its server here, such as GitLab's base URL. Added after M4; see decisions [0050](decisions/0050-profiles-hold-logins-connections-and-settings.md) and [0054](decisions/0054-module-settings-replace-connections.md). | after M4 |
| `server()` | The module's server in the item's profile, or null for a module without a `ServerType`. See [servers and tokens](#servers-and-tokens). | M5 |
| `status()` | This item's entry in the status cache: read the current status, refresh, subscribe. Null for a type without status. See [status](#status). | M5 |
| `createWebView(url)` | A live web page in a plain `QWidget`, in the web logins of the item's [profile](web-views.md#profiles), with popups, permissions, downloads and the toolbar handled by the core. The normal way for a web type to show a page. See [decision 0037](decisions/0037-core-makes-web-views.md). | M4 |
| `profile()` | The web logins of the item's [profile](web-views.md#profiles), as a `QWebEngineProfile`, for a module that builds its own `QWebEngineView`. Such a module links Qt WebEngine with `QT_LIBS`, and handles popups, permissions and downloads itself. The profile holds the logins of every site that its items visit, including those of other modules; see [decision 0042](decisions/0042-user-managed-web-profiles.md). | M4 |
| `createMessageView(text)` | A grey message in the cell, such as "Set the issue key in the item's settings." | M4 |
| `folderPath(folderId)` | The local path of a project folder on this device, or empty if not mapped. An empty id means the main folder, from M7. | M4 |
| `mapFolder(folderId)` | Asks the user for a folder's local path on this device, and stores it | M7 |
| `gitHead(path)` | The current branch of a folder, read from its files. See [decision 0080](decisions/0080-git-read-by-the-core.md). | M7 |
| `gitStatus(path, receiver, done)` | Runs `git status` in the background and reports a `GitState` | M7 |
| `launchTerminal(dir, program, arguments)` | Starts a program in a terminal, or a terminal alone. See [terminal launch](integrations.md#terminal-launch) and [decision 0082](decisions/0082-commands-as-argument-lists.md). | M7 |
| `startProgram(dir, program, arguments)` | Starts a program, such as an editor, detached | M7 |
| `showFolder(path)` | Shows a folder in the file manager | M7 |
| `onConfigChanged(receiver, changed)` | Reports a change to the type's `viewKeys()`, which does not make the view again. See [decision 0081](decisions/0081-views-that-keep-their-own-keys.md). | M7 |
| `updateConfig(config)` | Changes the item's configuration through `Model` | M4 |
| `showPage(url)` | Shows a web page in place of the view, with a way back. See [decision 0069](decisions/0069-open-page-in-the-placement.md). | M6 |
| `addItemToTask(typeId, name, config)` | Adds an item to the item's task, for "Add to task as item" | M6 |

### Servers and tokens

A module that talks to a server keeps its base URL in a settings screen, with a `Secret` field for its token on the same screen. Each profile has its own; a view reads them with `moduleSettings(screenId)`. See [decision 0054](decisions/0054-module-settings-replace-connections.md).

From M5 a module describes its server with a `ServerType`, in `ApiClient.h`, and registers it with `setServerType()`. The type names the screen and the token field, reads the base URL from the screen's values, says what is missing, such as Jira Cloud's email, and adds the token to a request, such as GitLab's `PRIVATE-TOKEN` header. The core knows nothing else about the service.

```cpp
class GitLabServerType : public mirante::ServerType
{
public:
    QString screenId() const override { return QStringLiteral("general"); }
    QString tokenKey() const override { return QStringLiteral("token"); }
    QUrl baseUrl(const QJsonObject &values) const override { return baseUrlOf(values); }
    void authorize(QNetworkRequest &request, const QString &token, const QJsonObject &) const override
    {
        request.setRawHeader("PRIVATE-TOKEN", token.toUtf8());
    }
};
```

The core then gives the module an `ApiClient` for the server of each profile, through `ItemContext::server()` in a view and `StatusJob::server()` in a status fetch:

- **`get()`, `post()` and `put()`:** the normal way to call an API. The path is relative to the base URL. The client adds the token, sends the last ETag, follows `Link` pages when asked, waits on `429` and on a rate-limited `403`, and runs at most four requests at once for each profile's server. Item code never sees the token.
- **`authorize(request)`:** adds the authentication to a `QNetworkRequest` the module makes itself.
- **`secret()`:** returns the raw token, asynchronously. Use it only when a request is not enough, such as passing a token to a command in a terminal. Every call needs a comment that says why `authorize()` does not work there.

Tokens are loaded from the keychain, under `<workspace id>/<profile id>/<module id>/<field key>`, before the first request. If no token is stored, a request fails with `NoToken` without being sent, and the cell shows "Token not set", as described in [errors](integrations.md#errors).

### Status

An item type that shows one object on a server, such as a merge request, reports its status. It returns true from `hasStatus()` and implements `fetchStatus(StatusJob &job)`, from `Status.h`. The [status cache](integrations.md#polling-and-caching) decides when an item is due, and makes the job. The type reads its configuration and settings from the job, makes its requests through `job.server()`, and ends with `job.finish(status)`, exactly once.

An `ItemStatus` holds a state, a one-line summary, the data a view needs, the attention reasons and whether the work is closed. `statusFromReply(reply)` turns a failed reply into the matching state. Each `AttentionReason` has a stable key, a text, and two flags: `notify`, for a desktop notification, and `clearsOnView`, for an event that stops counting once the user has seen the item. See [decision 0061](decisions/0061-attention-from-status-of-any-rendering.md).

A view reads its item's status with `ItemContext::status()`, and subscribes to its changes.

### The view kit

Native views of different modules need the same small widgets, and modules cannot depend on each other. The library `mirante_module_widgets`, linked through `mirante::module_api`, holds them under `<mirante/widgets/...>`. It uses only Qt, never core headers.

- `StatusPill` and `statusColours()`: a pill for a `StatusRole`. A module names a role, never a colour.
- `FlowRow`: a layout that wraps.
- `SummaryView`: the fixed layout of a summary, with sections that can be hidden. See [decision 0066](decisions/0066-fixed-native-layouts-with-sections.md).
- `ActionRow`: the buttons of a view, the "Web" link, and the error of the last action. `addLinkAction()` adds an entry that goes somewhere and changes nothing, such as "Open page". It looks like a link, so it stands apart from the commands.
- `styleAsLink()`: makes a button look like a link, as the links in a list's rows do.
- `askComment()`: asks for the text of a comment.
- `ItemListView`: a sortable table. See [decision 0071](decisions/0071-lists-link-out-without-a-preview.md).
  - A `ListCell` can hold `links`, each with an id and a label. A cell draws them as links, and a click emits `rowLinkActivated(key, linkId)`. Return on a row activates its first link. A column that holds links takes `hidePriority` 0, so that it never hides by width.
  - `setPageSize(n)` shows `n` rows at a time, with Previous and Next and a line such as "1-50 of 100". Zero or less means no pages, and the controls hide when all rows fit. `visibleKeys()` gives the current page's keys in table order, and `visibleKeysChanged()` says when they change.
  - `addFilter(column, label)` adds a drop-down above the table, filled from that column's values, with "All" first. The choice survives `setRows()` while its value exists, and otherwise falls back to "All".
  - Sorting and filtering cover all rows. A new sort or filter goes back to the first page. New rows keep the page, or move to the last page if there are now fewer.
  - `setDescription(text)` shows a line above the table that says what the list shows, such as "Of merge request !42 · acme/backend". Each list type sets it from its configuration.
  - The object names `filter.<column>`, `previousPage`, `nextPage` and `pageLine` let tests find the controls.
  - A selected row keeps its highlight across new rows, sorts and pages, so that Return acts on the row the user chose.
- `compactDate()`: a date for a table, in local time: `10-30 14:32` in this year, `2025-10-30` otherwise.
- `MarkdownView`, planned: a read-only `QTextBrowser` that shows Markdown with GitHub's extensions. Web and mail links open in the external programs. Loading images is off by default, as the Notes preview needs. A view that turns it on gives a base location for relative images and a way to fetch remote ones. The Notes preview moves to it when the Document items are built. See [decision 0104](decisions/0104-document-items.md).

### Testing

A module's tests also link `mirante::module_api_testing`. It holds `<mirante/testing/FakeApiClient.h>`, an `ApiClient` that answers from a table of recorded replies and records the requests, `<mirante/testing/FakeStatusJob.h>`, and `<mirante/testing/FakeQuickAdd.h>`. Recorded responses live in the module's `tests/data/` folder.

## Quick add

Quick add is generic. The core passes the pasted text to every quick add handler. Each handler can:

- recognise the text, such as a merge request URL or a Jira key;
- propose a task title and its items;
- name a project setting and value to match, such as `gitlab.project = acme/backend`. The core picks the project whose setting has that value. If none matches, or several do, it asks the user. When the items go to an existing task, the task's project is used and the match is ignored.

The core then passes the task's title and its items' text, such as a branch name, to every related-item finder. A finder can offer more items. For example, the `gitlab` module finds merge requests whose branch contains a Jira key, and the `jira` module recognises a Jira key in a merge request title.

The core either creates a task with the chosen items, or adds them to an existing task. See [decision 0083](decisions/0083-quick-add-into-a-task.md).

This lets the `jira` and `gitlab` modules work together without depending on each other. See [ui](ui.md#quick-add) and [decision 0078](decisions/0078-quick-add-dialog.md).

```cpp
class GitLabQuickAddHandler : public mirante::QuickAddHandler
{
public:
    void propose(QuickAddRequest &request) const override
    {
        // Not a merge request address: finish at once, without asking the server.
        // Otherwise ask request.server(), with request.owner() as the receiver,
        // and finish with a QuickAddProposal: title, items, project match, texts.
        request.finish(std::nullopt);
    }
};

void registerModule(ModuleRegistrar &registrar)
{
    registrar.addQuickAddHandler(std::make_unique<GitLabQuickAddHandler>());
    registrar.addRelatedItemFinder(std::make_unique<GitLabRelatedItemFinder>());
}
```

- **Requests.** A handler gets a `QuickAddRequest`, a finder a `RelatedItemRequest`. Like a `StatusJob`, each gives the module's settings and server, in the profile the dialog shows, and ends with `finish()`, exactly once. A run that does not finish in 30 seconds counts as finding nothing.
- **Proposals.** A `QuickAddProposal` has the task's title, its items in order, an optional `ProjectMatch` of one of the module's project settings, and texts for the finders, such as a branch name.
- **Items.** A `QuickAddItem` has a type id, a name and a configuration, as the item dialog would make. Its profile is empty, so it follows its group. The core drops an item whose type and configuration are already offered, or, when adding to a task, that the task already has. Finders see the task's items among the offered items.
- **Finders** also read the chosen project's settings with `projectSetting()`, such as the GitLab project to search.
- **Tests.** `<mirante/testing/FakeQuickAdd.h>` has `FakeQuickAddRequest` and `FakeRelatedItemRequest`.

## Attention

Item types report attention reasons through `status()`. A reason has a text, such as "Pipeline failed", and two flags:

- `notify`: also send a [desktop notification](ui.md#tray-and-notifications);
- `closed`: the item's work is finished, such as a merged merge request or a closed Jira issue.

The core suggests marking a task done when every item whose type reports `closed` reports it as true. See [data model](data-model.md#task).

## Missing modules

Synced data can reach a build that does not include a module. For example, one device builds without `jira`. The core then:

- **Items:** shows a placeholder: "This item needs the jira module, which this build does not include." The item's `type` and `config` are kept unchanged.
- **Removed types:** an item whose module is in the build but no longer registers its type, such as `claude.session.local` ([decision 0084](decisions/0084-no-local-claude-sessions.md)), is handled the same way. Its placeholder says "This item type is no longer available: claude.session.local." The item dialog changes only its name.
- **Project settings:** keeps the module's settings unchanged and does not show them.
- **Module settings:** keeps the values of the module's settings screens unchanged in each profile in `workspace.mirante`. The Profiles menu has no entry for the module.
- **Cells and Focus:** show the item's name. No status or attention is computed for it.

Everything is written back exactly as it was loaded. See [sync](sync.md#the-data-folder).

## Rules for module code

- Use only the module interface and Qt. Do not include core headers.
- Use another module's headers only if that module is listed in `DEPENDS`. Prefer quick add handlers and related-item finders, which need no dependency.
- Platform-specific code belongs in the core's `platform/`. If a module needs something new from the platform, add it to `ItemContext` first.
- Everything runs on the main thread. See [threading](architecture.md#threading).
- Each module tests at least its URL recognition and its attention rules, on recorded API responses.
- Each module's `README.md` lists its item types, settings and settings screens.
- Texts follow [texts](#texts).

## Texts

The app speaks English and Brazilian Portuguese ([decision 0086](decisions/0086-english-and-brazilian-portuguese.md)).

- Every text the user sees uses `QCoreApplication::translate()` with the module id as its context, such as `QCoreApplication::translate("gitlab", "Open")`. This includes the module's name, item type names, field labels, choice labels and messages. Module code does not call `tr()`, because its views have no `Q_OBJECT` of their own, so `lupdate` would file the text under a context that the running app does not ask for. `task check` fails on a `tr()` in module code.
- The source text is written out in the call, not passed through a helper function, so `lupdate` finds it. A text chosen in one place and translated in another is marked with `QT_TRANSLATE_NOOP("<module id>", "…")`.
- Texts made from parts use `arg()`, such as `QCoreApplication::translate("jira", "%1 more").arg(count)`, never string addition.
- Each new text is translated in the same change, in `translations/mirante_pt_BR.ts`. See [translations/README.md](../translations/README.md).
- Ids, stored values, API values, URLs and log messages stay in English.
- Dates and numbers shown to the user are formatted with the default `QLocale`, such as `QLocale().toString(date, QLocale::ShortFormat)`, or with `widgets::compactDate()`.

## Adding a module

1. Create `src/modules/<id>/` with a `CMakeLists.txt` that calls `mirante_add_module()`.
2. Write `registerModule(ModuleRegistrar &)` in the namespace `mirante::modules::<id>`.
3. Give the module a name with `setModuleName()`. Register the item types and any settings screens, settings and quick add handlers. Give each item type a short label with `shortName()`, such as "MR", for its cell headers. If a type shows something that has a web address, implement `fromUrl()`; see [items from a link](#items-from-a-link).
4. Add tests and a `README.md`.
5. Add the module's item types to [item types](item-types.md) and the module to the table at the top of this document.
6. Build once with the module on and once with it off. Both must build and pass the tests.
