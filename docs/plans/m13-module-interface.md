# M13 code plan: module interface before the split

This plan builds [milestone M13](../roadmap.md#m13-module-interface-before-the-split). After M13, every configuration screen a module can add also accepts a custom one, the interface holds nothing that no module uses, module tests need no core code, and a token goes only to the origin of its server. See decisions [0119](../decisions/0119-module-interface-versions-and-checks.md), [0120](../decisions/0120-fields-are-a-shortcut.md) and [0121](../decisions/0121-modules-are-trusted-code.md), and [issue 14](https://github.com/rrgmc/mirante/issues/14).

The interface version and the CI checks of decision 0119 come with the split, not here.

## Decisions

Two new decisions are written with the code:

- **Decision 0122: custom configuration screens.** It records the choices listed under "Configuration screens" below.
- **Decision 0123: tokens stay on their origin.** It records the choices listed under "Tokens" below.

The naming rule for third-party ids goes in [modules](../modules.md#ids), as the roadmap says.

### Configuration screens

- **One way to give a custom editor.** `ItemType::createEditor()` already takes an `EditorContext`. `SettingsScreen::createEditor()` gets the same signature. Project settings and device settings take a factory, with `ModuleRegistrar::setProjectEditor()` and `setDeviceEditor()`. Only tests call the old `SettingsScreen::createEditor(nullptr)`.
- **`Field` still describes the data, also with a custom editor.** A custom editor replaces the form, and the declared fields keep their other jobs: defaults, validation and finding secrets. A custom settings screen that edits a token must list it as a `Secret` field. The core uses that list to load the token's state and to delete the token with its profile. A forgotten entry would otherwise leave a token in the keychain.
- **A custom editor can write a token but never read it.** The token's abilities move from `FieldForm` to `ItemEditor` as virtual methods with defaults: `secrets()`, `removedSecrets()` and `setSecretState()`. `EntityActions` calls them on any editor, so it no longer needs `qobject_cast<FieldForm *>`. This keeps to decision 0121, which removes raw token access from the interface.
- **Secrets stay on settings screens.** A token's key holds a profile, and a project or a device has no profile of its own. The core ignores `secrets()` from a project or device editor, and `addProjectSetting()` and `addDeviceSetting()` keep refusing `Secret` fields. The roadmap's "including secrets" therefore applies to settings screens.
- **The editor knows its profile.** `EditorContext` gains `profileId`, and `ItemEditor` gains `setProfile()`. The item dialog calls it when the user picks another profile, and the project dialog calls it when the user picks another group. The core still builds its own form again in that case, as it does today. A module's editor stays and is told.
- **`ItemEditor` gains a `changed()` signal,** moved from `FieldForm`. The settings window saves as the user types, so a device editor must emit it, or nothing is saved. `ItemEditor` therefore gets `Q_OBJECT`, and the header joins the sources of `mirante_module_widgets` for moc.
- **A single custom field is set on the form, not in `Field`.** `FieldForm::setFieldEditor(key, FieldEditor *)` replaces one row's editor. `FieldEditor` is a small `QWidget` base with `setValue()`, `value()`, `problems()` and `changed()`. A type returns a `FieldForm` from `createEditor()` and sets the one field, which takes about three lines. Decision 0120 keeps `Field` as a description of data, and a widget factory in `Field` would make every copy of a field carry UI code.
- **`Field` gains `help` and `group`, both last and both empty by default.** Help text is a small grey line under the editor. Fields with the same `group` sit in one titled box, in the order the groups first appear, and fields without a group come first.
- **No condition for showing a field.** The roadmap adds one only if a screen needs it, and none does.
- **No first-party module uses the new abilities yet.** No current screen needs them. A test module covers them. Existing screens do not change.
- **Device setting types widen.** `addDeviceSetting()` now takes every type except `Secret`, `Folder` and `Image`. `DeviceSettings` already stores any JSON value, and a custom device editor would otherwise be stuck with four types. A device editor's keys must be declared, because `ItemContext::deviceSetting()` answers only declared keys and the settings window reads by key.
- **What the project and device editors get from the context.** A project editor gets the project's folders as they were when the dialog opened, `saveImage`, `loadImage`, `saveFile`, `folderPath` and `profileId`. `ProjectDialog` takes an `EditorContext` for this. A device editor gets an empty context, because no project or profile applies. Declared project fields keep refusing `Folder` and `Image`; a custom editor can use them through an inner `FieldForm`.

### The view kit

- **What moves into the kit,** with each class in the namespace `mirante::widgets`: `FieldForm`, `FieldEditor`, `ImagePasteDialog` and `ImageCropView`. `FieldForm` needs the last two. Their tests move to `tests/module_widgets/`, and their translation contexts in `mirante_pt_BR.ts` are renamed, with the translated texts unchanged.
- **`ImagePaste::imageFromMime()` becomes `widgets::imageFromMime()`.** The `ImagePaste` class needs the model and the store, so it stays in `src/ui` and calls the kit.
- **`validate()` and `defaultConfig()` move to `<mirante/FieldValidation.h>`,** implemented in `mirante_module_widgets`. The `FieldValidation` translation context stays.
- **The id rules move to `<mirante/ModuleIds.h>`,** so the registry and the test helper apply the same rules.

### Interface cleanup

Each member was searched for across `src`, `tests` and `docs`. No module uses any of them. What else goes with each one:

| Member | Other users | What goes with it |
|---|---|---|
| `ItemContext::itemName()` | Only `tst_placement` and the fake contexts | The `ItemContextImpl` override. `CellFrame` has its own `itemName` variable, which stays. |
| `ItemContext::profile()` | `ItemContextImpl::createWebView()` | It becomes a private non-virtual method of `ItemContextImpl`. The `QWebEngineProfile` forward declaration goes. |
| `ApiClient::authorize()` | `tst_server_client`, `FakeApiClient`, `StatusCache`'s client | The overrides and tests. `ServerType::authorize()` stays: it is how a module adds its token. |
| `ApiClient::secret()` | `FakeApiClient`, `StatusCache`'s client | The overrides. `ServerClient` keeps its own private token loading. |
| `ItemType::singleView()` | `CellFrame`, one test type | Only the member. The user chose to keep `CellFrameSingleView`, `SingleViewTracker` and their test in the core, so they are easy to bring back. `CellFrame` learns from its creator whether an item has one view, and the app always says no. |
| `ItemType::reparentSafe()` | `DashboardCellFactory::canReparent()` | Only the member and that override. `CellFactory::canReparent()`, the branches in `LayoutView` and their tests stay, and the base class's default of true applies. |

Also:

- **Documentation.** `Values.h`, `item()`, `task()`, `project()`, `group()` and the "read-only views" sentence leave [modules](../modules.md). The attention section is rewritten from `Status.h`: `notify` and `clearsOnView` are the flags of `AttentionReason`, and `closed` belongs to `ItemStatus`. `singleView` and `reparentSafe` leave [item types](../item-types.md) and [architecture](../architecture.md). Accepted decisions and old plans are not rewritten.
- **`FakeItemContext`,** in `<mirante/testing/FakeItemContext.h>`, replaces the 17 hand-written contexts in module tests. It has public members for what a test sets (`itemConfig`, `settings`, `projectSettings`, `folders`, `files`, `client`) and records what a view does (`webViewUrls`, `shownPages`, `mapped`, `addedItems`). Tests that need more derive from it. `tests/items/tst_dash_panel.cpp` is core code and keeps its own context, without the removed members.
- **`TestRegistrar`,** in `<mirante/testing/TestRegistrar.h>`, is a `ModuleRegistrar` for one module with the same read methods the tests already call on the registry, such as `types()`, `type()`, `contains()`, `moduleName()`, `settingsScreens()`, `serverType()` and `setSettingsReader()`. It applies the same id rules and stops on a broken one as the registry does. This keeps each test's change to one or two lines.
- **`usesProfile()` is core logic.** The three module tests that call it check `rendering()` and `settingsScreens()` instead, and `tests/items/tst_registry.cpp` keeps covering it.
- **`mirante_add_module()` stops linking `mirante_items` to tests.** `tools/check-module-includes.py` stops skipping module tests. It allows `../` there, since tests include their module's files that way, and refuses core headers.
- **`DEPENDS`.** Each module gets an interface target, `mirante_module_<id>_headers`, holding its source folder as an include path. `mirante_finish_modules()` links the dependant to the other module's library and to that target, once all modules are known. A module includes a dependency's header by its file name, and only modules listed in `DEPENDS` are reachable. The app target gets none of these paths.
- **Testing `DEPENDS`.** No first-party module uses it, so a CTest test configures and builds a small project in `tests/cmake/depends/`. Two modules there check that the include and the link work. It uses `cmake/MiranteModule.cmake` with a stub interface target. A third module, which includes a header without listing the module, must fail to build.
- **Third-party ids.** The rule in [modules](../modules.md#ids): a third-party module id starts with its owner's name, such as `acmetickets` for the module `tickets` of the owner `acme`. Ids stay lowercase letters and digits, because the dot separates a module id from the rest of an item type's id. The ids of the first-party modules, and `dash`, are reserved. An id is stored in users' data, so it never changes.

### Tokens

- **A request refuses another origin.** `ServerClient::enqueue()` compares the resolved URL with the base URL using the existing `sameOrigin()`, and on a mismatch answers without sending. This also catches a path such as `foo:bar`, which `QUrl` reads as a scheme.
- **A new error, `ApiReply::Error::OtherServer`.** The user should see what happened, and the existing errors mean something else. `statusFromReply()` maps it to `State::Error`, so the server is not marked as failing.
- **Redirects.** The test comes first, against two local `QTcpServer` ports, because a fake network cannot show what Qt does. The custom header is expected to follow a redirect under Qt's default policy. If so, `ServerClient::send()` sets `UserVerifiedRedirectPolicy`, and a slot on `QNetworkReply::redirected` allows a redirect on the same origin and otherwise aborts and answers `OtherServer`.
- **A redirect to another origin is refused, not followed without the token.** A request without the token would mostly fail anyway, and no module expects such a redirect. The same applies to `http` to `https` on one host, which counts as another origin. The message names the address, so the user can correct the base URL.

The document module's downloads carry no token, so they keep their policy.

## Parts

Each part is one commit or a few, with its documents, and the build and tests pass after each.

1. **Tokens.** The origin check, the redirect test, the policy, decision 0123, and [integrations](../integrations.md). It closes issue 14.
2. **Moves into the kit.** `FieldValidation`, `ModuleIds`, `ImagePasteDialog`, `ImageCropView`, `imageFromMime()` and `FieldForm`, with no change in behaviour. Tests and translation contexts move too.
3. **`ItemEditor` abilities.** `changed()`, the secret methods, `setProfile()` and `EditorContext::profileId`. `EntityActions` works through `ItemEditor`. `SettingsScreen::createEditor()` gets the context.
4. **Custom project and device editors.** `setProjectEditor()`, `setDeviceEditor()`, `ProjectDialog`'s context, `SettingsDialog`, wider device setting types, tests in `tst_dialogs` and `tst_settings_dialog`, and decision 0122.
5. **`FieldForm` additions.** `FieldEditor`, `setFieldEditor()`, `help` and `group`, with tests in `tests/module_widgets/tst_field_form.cpp`, and the texts in [modules](../modules.md).
6. **Testing library.** `FakeItemContext` and `TestRegistrar`, then all module tests move to them, `mirante_items` leaves the module tests, and the include check covers them.
7. **Removals.** The members in the table and the code that goes with them, with [modules](../modules.md), [item types](../item-types.md) and [architecture](../architecture.md). This comes after part 6, so the fake contexts change in one place.
8. **`DEPENDS` and ids.** The CMake change, the CTest test, and the naming rule.
9. **Finish.** The attention section, the module READMEs that mention what changed, the roadmap's M13 boxes, `tools/build.ps1 -Preset ci`, and the pull request.

## Platforms

No part changes code in `src/platform/`. The only macOS-specific lines near this work are `Qt::WA_LayoutUsesWidgetRect` on group boxes. Field groups use the same attribute, so that a box title does not overlap the widget above it. CI does not check macOS during development, so the pull request will say so, and field groups need a look on macOS at the next build by hand.

## Testing

- **Tokens:** a full address in a path on another host sends nothing and answers `OtherServer`. The same holds for each of `get()`, `post()` and `put()`. A redirect on the same origin still works. A redirect to another port does not reach that server with the token.
- **Editors:** a custom settings screen's typed token reaches the store, its Remove works, and its state is shown. A profile deletion removes its token. A custom project editor's values are stored, and a section not shown leaves them alone. A custom device editor saves on `changed()`. `setProfile()` is called when the profile or group changes.
- **`FieldForm`:** a custom field's value is read, written and validated, and an untouched one is kept unchanged. Help and groups are shown, and config keys do not depend on grouping.
- **Removals:** the build is the test. The layout tests keep passing without the removed paths.
- **Modules:** each module's tests pass with `TestRegistrar` and without `mirante_items`, and `task check` passes with all modules and with none.
