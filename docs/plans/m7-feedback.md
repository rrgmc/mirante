# M7: changes after trying it

The user tried [M7](m7-quick-add-and-local-items.md) and asked for these changes. Each task below is one branch.

## Quick add into a task (Q)

[Decision 0083](../decisions/0083-quick-add-into-a-task.md).

- **Names.** File > **New Task from Link…** (`Ctrl+Shift+N`) and the sidebar header button of the same name replace "Quick Add". The dialog's title is "New Task from Link", and its button **Create Task**.
- **Into a task.** **Add Items from Link…** in the context menu of a task and of its Items row. The dialog has no Title or Project; the project is the task's, and the profile starts as its group's. Finders run as soon as a proposal arrives. The button is **Add Items**. Items whose type and configuration the task already has are not offered.
- **`QuickAddRun`** gains `addToTask(taskId, items)`, which adds the items in order through `edits::addItem` and changes no dashboard, and returns their ids. `QuickAddDialog` gains a way to open for a task.
- **Tests:** `tst_quick_add_run` (adding to a task, items already in the task left out), `tst_quick_add_dialog` (the task mode's widgets, title and button, and that the finders run with the task's project).
- **Documents:** [ui](../ui.md#quick-add), [modules](../modules.md#quick-add), [architecture](../architecture.md), the M7 checks.
- **Owns:** `src/items/QuickAddRun.*`, `src/ui/QuickAddDialog.*`, the quick add parts of `MainWindow.cpp` and the task menus in `EntityActions.cpp`, their tests.

## No Claude session (local) (C)

[Decision 0084](../decisions/0084-no-local-claude-sessions.md).

- Remove `claude.session.local`, `LocalSessionView.*`, its tests, and the `program` device setting from the `claude` module.
- Remove `ItemContext::deviceName()`, `DeviceSettings::deviceName()` and the device name in the settings window.
- **Removed types.** An item whose type is not registered, but whose module is in the build, says "This item type is no longer available: <type id>." in its cell, not that the module is missing. Its data is kept unchanged. Test it in `tst_placement`.
- **Documents:** [item types](../item-types.md), [modules](../modules.md), [integrations](../integrations.md), [sync](../sync.md), [ui](../ui.md#settings), [decision 0008](../decisions/0008-claude-sessions.md)'s status, the roadmap's M7 box, the `claude` module's README and the M7 checks.
- **Owns:** `src/modules/claude/`, `src/items/ItemContextImpl.*`, `src/items/UnknownItemType.*`, `src/core/DeviceSettings.*`, the Terminal section of `src/ui/SettingsDialog.cpp`, `src/module_api/mirante/ItemContext.h`, their tests.

## Images dropped from other apps (D)

[Decision 0085](../decisions/0085-drop-images-from-other-apps.md).

- **Reading a drop:** one function, in `ImagePaste`, that takes a `QMimeData` and returns the image: the first local file whose suffix is png, jpg, jpeg, gif, bmp or webp and that `QImageReader` reads, else `imageData()`. A test covers files, image data, other files and an unreadable image.
- **Sidebar:** a task row or its Items row accepts such a drop alongside the sidebar's own drags, and opens the paste dialog with the image, then adds an Image item as Paste Image… does.
- **Image cells:** a cell whose item is an Image accepts the drop, opens the paste dialog, and replaces the image as `Ctrl+V` does. Other cells refuse it.
- **Item dialog:** an image field accepts the drop, as its Paste button.
- **Tests:** drops simulated with `QDropEvent` on each place, and refused drops elsewhere.
- **Documents:** [ui](../ui.md#pasting-images), [item types](../item-types.md#image), the M7 checks.
- **Owns:** `src/ui/ImagePaste.*`, `src/ui/SidebarTree.*`, `src/ui/FieldForm.cpp`, the image drop parts of `MainWindow.cpp` and `src/layout/CellFrame.*` or wherever cells take drops, their tests.

## English and Brazilian Portuguese (L)

[Decision 0086](../decisions/0086-english-and-brazilian-portuguese.md). Starts after Q, C and D are merged, because they change texts.

- **Qt:** add `qttools` and `qttranslations` to `tools/setup-qt.ps1`, `tools/setup-qt.sh` and CI.
- **Build:** `find_package(Qt6 ... LinguistTools)`, `qt_add_translations()` over the app's and modules' sources, compiled into the app's resources. `task translations` runs `lupdate`. `task check` fails on unfinished translations.
- **Start:** `main.cpp` reads the device setting `language` (`system`, `en` or `pt_BR`), picks the language as the decision says, and installs Mirante's translator and Qt's `qtbase` translator before any window is made. Tests set English.
- **Settings window:** a **Language** choice in its first section, with "Applies at the next start."
- **Translation:** every text in `translations/mirante_pt_BR.ts`, in Brazilian Portuguese, with the glossary terms at the top of the file. Check that the texts that are built from parts, such as "%1 was not found.", use `arg()` and not string addition, so the order can change.
- **Locale:** dates and numbers formatted for reading use the chosen language's `QLocale`.
- **Documents:** [ui](../ui.md#settings), [sync](../sync.md#device-settings), [architecture](../architecture.md), `docs/README.md` for contributors, the root `README.md`.
- **Owns:** everything it needs; it runs alone.

## Order

| Wave | Tasks |
|---|---|
| 1 | Q, C, D |
| 2 | L |

The orchestrator merges each branch after building and testing it with `master` merged in, then updates the M7 checks and pushes at the end.
