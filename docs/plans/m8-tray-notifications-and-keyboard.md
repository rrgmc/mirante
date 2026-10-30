# M8 code plan: tray, notifications and keyboard

This plan finishes [milestone M8](../roadmap.md#m8-tray-notifications-and-keyboard). Hiding to the tray and the global hotkey are already done, by [decision 0075](../decisions/0075-hide-to-tray-and-show-hide-hotkey.md). After M8, the tray icon shows how many tasks need attention, a failed pipeline sends a desktop notification that opens its task, every shortcut works inside web pages, and the File menu is tidied.

## Decisions

All accepted:

- [Decision 0087](../decisions/0087-watched-items.md): only watched items count on the tray and send notifications. Items start not watched. The choice is stored in the item and synced.
- [Decision 0088](../decisions/0088-keys-a-page-keeps.md): app shortcuts come first inside web pages, apart from WebEngine's editing keys. An item can give a list of keys to its page.

Smaller choices made here:

- **The tray counts tasks,** as [ui](../ui.md#tray-and-notifications) says: active tasks, not in the trash, with at least one watched item that needs attention. A click on a notification opens a task, so the task is the unit the user acts on. The sidebar keeps counting items.
- **The badge** is a filled circle with the number, drawn in code on the corner of the icon. Above 99 it reads "99+". With no tasks it is the plain icon. The tooltip says "Mirante: 2 tasks need attention".
- **A notification is sent once per reason.** The keys of the reasons already notified are kept per item on the device, beside the seen keys of `Attention`. An item's first status with data records its notify keys without sending anything, and so does turning on Watch. The app never sends a burst for what was already true.
- **Many at once.** When more than three notifications are due in one pass of the event loop, one notification says "4 pipelines failed", and a click brings the window to the front.
- **Clicking a notification** shows the window, opens the task's first dashboard in the preview and selects the item in the sidebar, as a double click on the dashboard does. A task in the trash or deleted since then only shows the window.
- **Where notifications come from.** Windows and Linux use `QSystemTrayIcon::showMessage()` and `messageClicked`. Some Linux desktops do not report the click; the notification still shows. macOS uses `UNUserNotificationCenter`, which needs the app bundle the build already makes, and asks for permission the first time. Without a tray, as on GNOME without an extension, there are no notifications.
- **The File menu** puts creating first and moves the commands on the open workspace into a submenu. Open Sample Data moves to Help:

  ```text
  File                                      Help
    New Task from Link…   Ctrl+Shift+N        Modules…
    New Group…                                Open Sample Data
    ─────────────                             ──────
    New Workspace…        Ctrl+N              About Mirante
    Open Workspace…       Ctrl+O              About Qt
    Open Recent         ▸                     (Show Log)
    Workspace           ▸  Rename…
                           Move…
                           Show Folder
                           ──────
                           Reload       F5
    ─────────────
    Quit                  Ctrl+Q
  ```

  The actions, their shortcuts and what they do stay the same. Only their place and the shorter names inside the submenu change.

## Changes to the architecture

- **`Item`** gains `watch` (bool, written only when true) and `pageKeys` (a list of key sequences in `QKeySequence::PortableText`, written only when not empty).
- **`Attention`** gains `trayCount()`, `isWatched()` and the signal `notificationDue(itemId, reason)`. It keeps the notified keys in `DeviceSettings`, as it keeps the seen keys.
- **`StatusCache`** also polls the watched items of active tasks.
- **`platform::Tray`** gains `setCount(int)` and `showMessage(title, text)`, and the signal `messageClicked()`.
- **New `platform::Notifier`** sends notifications. On macOS it is `NotifierMac.mm` with `UNUserNotificationCenter`; elsewhere it calls the tray. It emits `clicked(id)` with the id given when the notification was sent.
- **New `web::PageKeys`**, an event filter for a web view's focus proxy that accepts `ShortcutOverride` for the item's keys.
- **`ItemDialog`** gains the Watch check box and the page keys list.

## Part 1: watched items

- `Entities`: `watch` and `pageKeys` in `Item::fromJson()` and `toJson()`. Absent fields read as false and empty, and are not written, so unchanged items stay byte for byte the same.
- `ItemDialog`: under the profile, a check box "Watch: count on the tray icon and notify". It is shown for types whose `hasStatus()` is true.
- Context menus of an item in the sidebar and in a cell header: a checkable "Watch". It writes through the Store, like a rename.
- `StatusCache`: an item is also due when it is watched and its task is active and not in the trash. It reads this from `Model`, and looks again on `taskChanged`, which also covers changes to its items, and `modelReset`.
- Tests: JSON round trip with and without the fields; a watched item in a Normal, hidden task is polled; one in a Done task is not.

## Part 2: the tray count

- `Attention::trayCount()`: the number of active tasks, not in the trash, with a watched item that `needsAttention()`. `countsChanged` already follows `taskChanged`, which covers Watch.
- `Tray::setCount()`: on Windows and Linux, a new `QIcon` with the badge painted at each size of the base icon. On macOS, the status item's button shows the number as its title, next to the template icon, because a painted badge cannot follow the menu bar's light and dark appearance.
- `MainWindow` sets the count and the tooltip on `countsChanged`.
- Tests: `trayCount()` with watched and unwatched items, Done and trashed tasks, and two items of one task, which count once. The badge painting is a pure function, tested on its pixel size and on "99+".

## Part 3: notifications

- `Attention` compares each status of a watched item with the notified keys. Each new reason with `notify` emits `notificationDue`, and the key is recorded. Keys no longer reported drop out of the record, so a pipeline that fails again later notifies again. The GitLab reasons already have per-pipeline keys.
- `MainWindow` collects the due notifications of one event loop pass, and sends them through `Notifier`: one per reason, with the reason's text as title and "Task / Item" as text, or the summary when there are more than three.
- `Notifier::clicked` opens the task as described above.
- Tests: no notification on an item's first status; one when a notify reason appears; none for an unwatched item or a reason without `notify`; none again after a restart, using the same `DeviceSettings`; one again when the key left and came back; none when Watch was just turned on. `Notifier` itself is not tested automatically.

## Part 4: keys in web pages

1. **Check first.** Before any code, try every key of [ui](../ui.md#keyboard) with the focus in five places: the sidebar, a native cell, a web page outside a text field, a text field of a web page, and a detached window. Use a release build; see the memory on debug WebEngine. Record the results in `docs/plans/m8-checks.md`. Decision 0088 assumes WebEngine keeps only editing keys; if the check shows otherwise, fix that before the rest.
2. Fix what the check finds. Shortcuts that live on menu actions of the main window may not run in a detached window; if so, the detached window adds those actions too.
3. `PageKeys`: `WebPanel` and `PageArea` install it on the view's focus proxy, which WebEngine makes after the first load, so they watch for it with `QEvent::ChildAdded`. It accepts `ShortcutOverride` when the key matches one of the item's `pageKeys`.
4. `ItemDialog`: for types whose views are web views, a "Keys for the page" list with Add and Remove. Add opens a `QKeySequenceEdit`. A key that no app shortcut uses is still accepted, but the list says it was free anyway.
5. Tests: `PageKeys` with a plain `QWidget` standing in for the focus proxy and synthetic `ShortcutOverride` events; the dialog keeps `pageKeys` and the other unknown keys.

## Part 5: the File menu

- `MainWindow`: the File menu as drawn above, and Open Sample Data in Help. The actions are the same objects, so their shortcuts, enabled state and the sample data's limits do not change.
- `ui.md`, [Menus](../ui.md#menus): rewrite the File and Help entries to match.

## Part 6: documents

In the same commits as the code:

- [ui](../ui.md): Watch in the item dialog and the context menus, the tray count, the notifications, the page keys, and the menus.
- [data model](../data-model.md#item): `watch` and `pageKeys`.
- [integrations](../integrations.md#polling-and-caching): watched items are polled.
- [web views](../web-views.md): the key handling of decision 0088.
- The decision index, and the roadmap boxes once the user has checked.

## Order of work

The parts are small and share `MainWindow.cpp` and `ItemDialog.cpp`, so one session does them in order, each on its own branch, merged when its build and tests pass. There are no parallel agents in M8.

| Order | Branch | Parts |
|---|---|---|
| 1 | `m8-watch` | Part 1 |
| 2 | `m8-tray-count` | Part 2 |
| 3 | `m8-notifications` | Part 3 |
| 4 | `m8-file-menu` | Part 5 |
| 5 | `m8-page-keys` | Part 4 |

The File menu comes before the keys because it is quick and the key check then covers the new menu.

## Checking

- **Automatic:** the tests above, by CTest locally and in CI on Windows and Linux, with all modules and with none.
- **By hand on Windows,** listed in `docs/plans/m8-checks.md`:
  - Watch on and off from the dialog and both context menus, and on a second instance;
  - the tray count going up and down, with the window hidden;
  - a failed pipeline on a watched item notifies once, also after a restart, and a click opens its task; an unwatched item does not notify;
  - the key table of Part 4, before and after the fixes, and a page key such as `Ctrl+K` given to a claude.ai item;
  - the new File and Help menus.
- **Not checked by hand:** the macOS badge and `NotifierMac.mm`. The user tries them at the next manual macOS build. Linux notifications are checked only if a Linux desktop is at hand.

## Out of scope

- Tasks that need attention listed in the tray menu.
- Notifications without a tray on Linux, through the freedesktop notification service.
- Notifications for reasons other than failed pipelines. The modules choose `notify`, and only failed pipelines set it.
- Suggesting "done" for a task, which the M6 and M7 plans left for later.
