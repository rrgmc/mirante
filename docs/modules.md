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

All modules are enabled by default. The item types are described in [item types](item-types.md).

Built-in panels (`dash.panel`) belong to the core, not to a module, because the `dash://` scheme and its bridge are core code. See [web views](web-views.md#built-in-panels).

## Folder layout

```
src/modules/gitlab/
  CMakeLists.txt         calls mirante_add_module()
  README.md              what the module does, its types and connection kinds
  GitLabModule.cpp       the register function
  GitLabClient.h/.cpp    module code, freely organised
  ...
  tests/                 Qt Test sources for this module
```

A module may add more folders inside its own folder. It never adds files outside it.

## Ids

- A module id is lowercase letters and digits only, such as `gitlab`.
- Every id a module registers is the module id, or starts with the module id and a dot. This applies to item types (`gitlab.mr.summary`), connection kinds (`jira.cloud`) and settings.
- The prefix `dash` is reserved for the core.

The prefix tells the core which module an unknown id belongs to, so a placeholder can name the missing module.

## Building

Each module's `CMakeLists.txt` calls one function, defined in `cmake/MiranteModule.cmake`:

```cmake
mirante_add_module(gitlab
    SOURCES GitLabModule.cpp GitLabClient.cpp ...
    TESTS   tests/tst_urls.cpp tests/tst_attention.cpp
    DEPENDS          # other module ids, usually none
    DEFAULT ON)
```

The function:

1. creates the option `MIRANTE_MODULE_GITLAB`, with the given default;
2. if the option is off, does nothing more;
3. builds a static library `mirante_module_gitlab`, linked only to the module interface and Qt;
4. adds the module's tests to CTest;
5. adds the module to the list of enabled modules.

The top-level `CMakeLists.txt` adds every folder in `src/modules/` that has a `CMakeLists.txt`. After all modules are added, it checks `DEPENDS`. If an enabled module depends on a disabled one, configuration stops with a message that names both.

**Choosing modules:**

- one option at a time: `-DMIRANTE_MODULE_JIRA=OFF`;
- an exact list: `-DMIRANTE_MODULES="gitlab;web;notes"`, which enables those modules and disables all others;
- a configure preset in `CMakePresets.json`, for builds that are used often.

**Registration.** CMake generates `enabled_modules.cpp` in the build folder. It calls each enabled module's register function, in alphabetical order of module id:

```cpp
void registerEnabledModules(ModuleRegistrar &r)
{
    mirante::modules::folder::registerModule(r);
    mirante::modules::gitlab::registerModule(r);
    // ...
}
```

`main.cpp` calls `registerEnabledModules()` once at startup. The generated file also lists the enabled module ids, which the About dialog shows.

Modules do not register themselves through static objects. The linker drops unreferenced objects from static libraries, so such registration fails silently on some platforms.

## The module interface

A module links to one CMake target, `mirante::module_api`. It holds the only core headers a module may include. A module that includes any other core header does not compile, because the include path is not available to it.

The module interface contains:

| Header | Contents |
|---|---|
| `Module.h` | `ModuleRegistrar`, the object a module registers its parts with |
| `ItemType.h` | The `ItemType` interface. See [item types](item-types.md#the-type-registry). |
| `ItemContext.h` | `ItemContext`, everything an item's code can reach. See [below](#item-context). |
| `Fields.h` | Field definitions: type, default, validation. Used for item configuration, connection fields and settings. |
| `ConnectionKind.h` | The `ConnectionKind` interface |
| `ApiClient.h` | The shared HTTP client base: ETags, paging, rate limits, request limits |
| `Status.h` | Status results and attention reasons for the [status cache](integrations.md#polling-and-caching) |
| `QuickAdd.h` | Quick add handlers and related-item finders |
| `Values.h` | Read-only views of a group, project, task, item and folder |

Modules see entities only through read-only views. Changes go through actions on `ItemContext`, which call the core's `Model` methods.

## What a module registers

The register function receives a `ModuleRegistrar` and adds any of the following:

| Part | Description |
|---|---|
| Item types | One object per type, with the properties listed in [item types](item-types.md#the-type-registry) |
| Connection kinds | Fields of the connection, how to sign a request, how to test the connection |
| Project settings | Fields shown in the project dialog, such as the GitLab project path. They are defaults for the module's items. |
| Device settings | Fields kept on each device, such as an editor command |
| Quick add handlers | Recognise pasted text and propose a task. See [quick add](#quick-add). |
| Related-item finders | Find items related to a piece of text, such as merge requests whose branch contains a Jira key |

The core builds every dialog from the field definitions. It stores and edits the values without knowing what they mean.

## Item context

Each item's code gets an `ItemContext`. It gives access to the configuration the core owns, without the core's classes.

| Member | Description |
|---|---|
| `item()`, `task()`, `project()`, `group()` | Read-only views of the item and where it sits |
| `projectSetting(key)` | A project setting the module declared, such as `project` for GitLab. Empty if the project does not set it. |
| `deviceSetting(key)` | A device setting the module declared |
| `connection(kind)` | The connection found by the [resolution order](data-model.md#connection), or an error: none set, token missing, or failing |
| `status()` | This item's entry in the status cache: fetch, read cached data, report attention |
| `profile()` | The web profile for a URL, for web item types |
| `folderPath(folderId)` | The local path of a project folder on this device, or empty if not mapped |
| `gitHead(path)` | The current branch of a folder |
| `launchTerminal(dir, command)` | Starts a command in a terminal. See [terminal launch](integrations.md#terminal-launch). |
| `updateConfig(config)` | Changes the item's configuration through `Model` |

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
- **Cells and Focus:** show the item's name. No status or attention is computed for it.

Everything is written back exactly as it was loaded. See [sync](sync.md#the-data-folder).

## Rules for module code

- Use only the module interface and Qt. Do not include core headers.
- Use another module's headers only if that module is listed in `DEPENDS`. Prefer quick add handlers and related-item finders, which need no dependency.
- Platform-specific code belongs in the core's `platform/`. If a module needs something new from the platform, add it to `ItemContext` first.
- Everything runs on the main thread. See [threading](architecture.md#threading).
- Each module tests at least its URL recognition and its attention rules, on recorded API responses.
- Each module's `README.md` lists its item types, connection kinds and settings.

## Adding a module

1. Create `src/modules/<id>/` with a `CMakeLists.txt` that calls `mirante_add_module()`.
2. Write `registerModule(ModuleRegistrar &)` in the namespace `mirante::modules::<id>`.
3. Register the item types and any connection kinds, settings and quick add handlers.
4. Add tests and a `README.md`.
5. Add the module's item types to [item types](item-types.md) and the module to the table at the top of this document.
6. Build once with the module on and once with it off. Both must build and pass the tests.
