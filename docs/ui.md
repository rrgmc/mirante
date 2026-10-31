# User interface

This document describes navigation, dashboards and editing. It was checked with the clickable HTML prototype in [milestone M1](roadmap.md#m1-html-prototype).

## Main window

```
┌───────────────┬──────────────────────────────────────────────┐
│ Workspace     │ [Morning] [Release] [+] ≡ │ [◎ Focus] [Fix…] 🔒 │
│ ▾ Work        ├──────────────────────────────────────────────┤
│   ▾ backend   │                                              │
│     Default   │        active root dashboard or task         │
│     ● Fix ... │        (splits, tabs and cells)              │
│     ○ Add ... │                                              │
│   ▸ frontend  │                                              │
│ ▸ Personal    │                                              │
│               │                                              │
└───────────────┴──────────────────────────────────────────────┘
```

- **Sidebar, on the left:** the workspace's name, then a tree of groups, projects and tasks. It has no Focus entry: Focus has its own slot in the tab row. See [decision 0053](decisions/0053-no-focus-entry-in-the-sidebar.md).
- **Tab row, on the right, above the content:**
  - On the left, the **tabs**: root dashboards only. The `+` button and the dashboard list button follow them. Dragging a tab along the row moves it among the other tabs. Task dashboards and items open in the preview slot, which never becomes a tab. See [decision 0052](decisions/0052-no-pinning.md).
  - On the right, after a thin separator, two fixed **slots**: **Focus** and the **preview**, then the lock. They look like tabs. See [decision 0048](decisions/0048-focus-and-preview-slots.md).
  - One page is current across the tabs and the slots. While a slot is current, no tab shows as selected, and the other way round.
- **Content area:** the page of the current tab or slot.
- **Warning bar, above the tab bar:** files that could not be loaded or saved. While the sample data is open, it also says that changes to it are not saved, and points to File > New Workspace…. See [decision 0029](decisions/0029-sample-data-read-only.md). It also has:
  - one line for each server that refused its token, such as "GitLab in the profile Work refused the token.", with a **Set Token…** button that opens the module's settings screen in that profile. The line goes when the server works again, or another workspace opens;
  - "Could not use the keychain: <message>" when the keychain refused a call. See [sync](sync.md#secrets).
- **Conflict bar, under the warning bar:** one line for each conflict copy a sync tool made, such as "Two versions of task Fix login timeout", with "Keep current", "Use other" and "Compare". See [sync](sync.md#conflict-copies).

The sidebar can be collapsed.

### Sidebar tree

- Groups appear in manual order.
- Projects and tasks appear in manual order. [Priority](data-model.md#priority) does not change their place; only Focus sorts by it. See [decision 0034](decisions/0034-manual-order-in-the-sidebar.md).
- A coloured marker shows the priority level. Projects have a small rounded square and tasks a circle, so the two levels are easy to tell apart. Low items are dimmed.
- Each task has two fixed children: **Dashboards**, with its task dashboards, then **Items**, with the task's items. They are there for every task, even when empty. Dashboards and items appear in the order the task stores them, which the user sets by dragging.
- The selected row is a rounded shape in the highlight colour, inset from the sidebar's edges and as wide as the sidebar. It keeps its full colour when the sidebar does not have the focus, so the dashboard on screen stays easy to see.
- Groups are headings: bold, a little larger than the rows under them, with the name in a pill of the [group's colour](data-model.md#group). The pill is a light tint of the colour with a thin edge in a stronger tint. On the selected row the selection shows instead.
- Four tabs at the bottom choose the task list. **Projects** lists active tasks under every project, including projects without any. **Active** lists the same tasks, but leaves out projects without active tasks. **Done** lists done and archived tasks under every project. **Trash** lists what is in the [trash](#trash). The first three lists leave out projects and tasks in the trash. A filter button right of the workspace title opens a menu with a checkbox for each priority. Unchecking one hides the projects and tasks of that priority in the Projects, Active and Done lists. The button looks pressed while it hides something. All priorities are shown at start.
- A `+` button in the sidebar header opens a menu: New Group…, New Project… in the group of the selected row, and New Task… in the project of the selected row. The last two are disabled when the selection has no group or project.
- Two buttons in the sidebar header expand or collapse every task at once, with its Dashboards and Items. Groups and projects are left as they are; they open and close by hand only.
- [Attention badges](#attention) appear on tasks, and are summed on projects and groups.
- **Drag and drop:**
  - Dragging a group, project or task among its siblings changes its place. It never changes a priority. Nothing is dragged in the Trash list.
  - Dragging an item among the items of its task, or a task dashboard among the dashboards of its task, changes its place in the task's list. It stays in its task. The same drag still drops the item or dashboard on a dashboard.
  - A task itself is not dragged onto dashboards. Drag one of its items, to show that item, or one of its dashboards, to show the task with that dashboard in a root dashboard. See [placing tasks](#placing-tasks-and-items).
- The context menu holds new, edit, rename, set priority, set state and delete, and on a task dashboard "Set as default". On a task and its Items row it also holds "Paste Image…", after "New item…", and then "Add Items from Link…". See [pasting images](#pasting-images) and [quick add](#quick-add). On a task dashboard or an item it also holds "Open in new root dashboard", which makes a root dashboard showing just that dashboard or item.
- Project rows have a light tint of their group's colour, a rounded shape like the selection. They stand out between groups and tasks, and show their group while its heading is scrolled away. Task rows have no tint. See [decision 0046](decisions/0046-group-colours.md).
- While the mouse is over a dashboard cell, the sidebar row of what the cell shows gets a soft tint: the item's row for an item cell, also one inside a task cell, and the task's row for a task cell. The tint goes when the mouse leaves the cell. The tree does not expand or scroll for it. The [settings window](#settings) can turn the tint off. If a collapsed row hides that row, the nearest row that can be seen takes the tint instead: the task of an item whose task or Items row is closed, or the project or group if that is closed. The tint follows as rows open and close.
- "Delete project…" and "Delete task…" move the project or task to the [trash](#trash), without a question, since it can be undone. Holding `Shift` while choosing them deletes for good, after a confirmation. Every other delete, such as of a group, an item or a task dashboard, asks for confirmation first and cannot be undone.

### Trash

Deleted projects and tasks go to the trash first. See [decision 0036](decisions/0036-trash-for-tasks-and-projects.md) and [data model](data-model.md#trash).

- The **Trash** tab at the bottom of the sidebar lists, under their groups, the projects in the trash with all their tasks, and the other projects that hold tasks in the trash, with only those tasks. Groups with nothing in the trash are left out. The priority filter does not apply here.
- The context menu of a project or task offers **Restore** and **Delete permanently…**. Delete permanently asks for confirmation first. Restoring a task whose project is in the trash restores the project too, with all its tasks.
- While the Trash tab is shown, an **Empty Trash…** button at the bottom deletes everything in the trash for good, after one confirmation that names the counts, such as "Permanently delete 1 project and 6 tasks in the trash?". The button is disabled when the trash is empty.
- A double click in the Trash list only expands or collapses a row. Nothing there opens.
- A project in the trash takes its tasks with it. They are left out of the other sidebar lists, [Focus](#focus-view), the [quick switcher](#keyboard), and the project and task pickers, such as the task dialog's project list.
- Tabs of a task in the trash close, as for a deleted task.
- A dashboard cell that shows a task in the trash, or an item of one, reads "In trash" instead of its content. It keeps its reference, so a restore brings the content back. A task cell changes at once; an item cell of a root dashboard changes when the dashboard is next built, such as when its tab opens.

## Group dialog

Creating or editing a group opens the group dialog. It has the group's name, its colour and its profile. OK is refused while the name is empty.

- **Colour.** A row of eight palette swatches, then "Other…", which opens the system colour dialog. The current colour is checked. A colour outside the palette gets a swatch of its own before "Other…". A new group starts with the first palette colour that no group uses yet. A group without a stored colour shows its palette colour, and stays without one unless another is chosen. See [decision 0046](decisions/0046-group-colours.md).

- **Profile.** "The default profile (<name>)" first, then the workspace's [profiles](#profiles), the default first, then by name. The items of the group use this profile for their logins and module settings, unless an item chooses its own. The first entry is stored as no value, so the group follows the default when another profile becomes the default. A profile that is not in the list, such as one deleted on another device, is shown as "Unknown profile, uses the default" and kept until another is chosen. See [web views](web-views.md#profiles).

- **A new profile for a new group.** When a group is created and other groups already exist, the dialog also offers "Create a new profile for this group", unticked. Ticking it disables the profile list and enables a "Profile name" field. The name starts as the group's name and follows it until it is edited. OK makes the profile, empty, and gives it to the group. A blank name, or one an existing profile already has, ignoring case, is refused with a message. The first group of a workspace is not offered this, since the default profile is there for it.

Groups have no connections. The server each module talks to is a module setting of a profile; see [decision 0054](decisions/0054-module-settings-replace-connections.md).

## Project dialog

Creating or editing a project opens the project dialog. It has the project's name, its group, its priority and its folders. OK is refused while the name is empty or a folder has no name.

- **Folders.** A table with each folder's name, remote URL, the Main check box and its path on this device. A folder without a path says "Folder not set on this device". "Choose…" picks the path. Exactly one folder is main when there are any. See [data model](data-model.md#folder).
- **Project settings.** After the folders, a module with project settings has a section if the project has values for it. The section is titled with the module's name, such as "GitLab" with "GitLab project path", or "Jira" with "Jira project key". An **Add settings** button lists the other modules by name, and choosing one shows its section. The button hides when nothing is left to add. Each section is a form of the fields the module declared. Items of the project's tasks use these values unless they set their own. An emptied field removes its value, and a section left without values is hidden the next time the dialog opens. Settings of a module that is not in the build are kept, though no section shows them. OK is refused while a shown section has a problem, and the message under the dialog names the module. See [data model](data-model.md#project).

## Profiles

Profiles > Manage Profiles… opens the list of the workspace's profiles; see the [Profiles menu](#profiles-menu). A profile holds web logins and module settings, such as the GitLab and Jira servers with their tokens. One profile is the default: groups and items that choose none use it. See [data model](data-model.md#profile), [web views](web-views.md#profiles) and decisions [0042](decisions/0042-user-managed-web-profiles.md), [0050](decisions/0050-profiles-hold-logins-connections-and-settings.md), [0054](decisions/0054-module-settings-replace-connections.md) and [0057](decisions/0057-any-profile-can-be-the-default.md).

- **Help.** A short text at the top says what a profile holds, that each group chooses a profile and each item can choose its own, that those that choose none use the default profile, that the list and the settings are shared with the workspace while logins and tokens stay on this device, and that a profile's module settings are in the Profiles menu.
- **Warning.** Under the help, a tinted box with the warning icon says: "Items in the same profile share its logins, servers and tokens. Every item that uses a profile can read the logins and data of all sites in that profile, and call its servers with its tokens. Use separate profiles for things you do not trust together."
- **The list.** The default first, with "(default)" after its name, then the other profiles by name.
- **New profile…** asks for a name. **Rename…**, or a double click, asks for a new one. Every profile can be renamed, the default too. A blank name, or one already used, ignoring case, is refused with a message.
- **Delete…** asks for confirmation first. It is disabled while the workspace has only one profile. The question says that the profile's module settings are deleted, and its logins, site data and tokens on this device. It names the groups and items that use the profile, such as "It is used by 1 group and 2 items. A group then uses the default profile, and an item its group's profile." For the default, it also names the profile that becomes the default: the first of the others by name. Groups and items that named it keep the id and fall back, so nothing else changes in their files.
- **Set as Default** makes the selected profile the default. It is disabled for the default itself.
- **Clear Data…**, at the right of the buttons, works on any profile. It first shows a warning with the warning icon: "Clear all web data of the profile "Work"?", explaining that its logins, cookies, cache and site data on this device are deleted, that every site in it signs out, that its pages reload, that its module settings stay, and that this cannot be undone. Cancel is the default button. After "Clear Data", pages in that profile show "Clearing this profile's data…" for a moment, then load again, signed out. The profile itself, its name, its settings and who uses it do not change.
- Changes to the list apply at once. The dialog closes with Close. Changes that arrive from another device update the list while it is open.

The item dialog offers a profile for items of types that use one; see [the type registry](item-types.md#the-type-registry). The views of an item are made again when its profile changes.

## Profiles menu

The Profiles menu, between View and Help, lists what can be done with profiles. See [decision 0054](decisions/0054-module-settings-replace-connections.md).

- **Manage Profiles…** (`Ctrl+P`) comes first. It opens the [profiles dialog](#profiles).
- A separator line follows.
- Then a submenu for each profile, named after it: the default first, with "(default)" after its name, then the others by name.
- Each profile's submenu holds what can be done with that profile. Its first entry is **Modules**, with a submenu for each module that has [settings screens](#modules), named after the module. That submenu lists the module's screens by their names, such as Profiles > Work (default) > Modules > GitLab > General Configuration…. When no module has screens, Modules is disabled and says "No module settings".
- **Set as Default** follows Modules. It makes the profile the default, and is disabled for the default itself.
- **Settings…** comes last, after a separator. It opens the profile's settings window, titled "Profile Settings: <name>". Its **Storage** section says where the profile keeps its data on this device, for this workspace: the **Web data** folder, with its logins, cookies and site data, and the **Cache** folder, each with an **Open** button that shows it in the file manager, and the start of the names of its **Tokens** in the keychain, such as `mirante/5a3e…/default/…`. A folder that no page of the profile has made yet cannot be opened. The fields can be selected and copied, not changed. Later sections may hold settings of the profile. The entry is disabled when the app runs without web profiles, as in some tests. One settings window is open at a time.
- The menu is built again when profiles are added, renamed or deleted, or the default changes, here or on another device.
- More entries for a profile, such as Rename… or Clear Data…, can be added to its submenu later. The code builds each profile's submenu in one function, `fillProfileMenu()` in `MainWindow.cpp`, where each entry is one line.

## Item dialog

"New item…", in the context menu of a task or its Items row, opens the item dialog in two steps. See decisions [0043](decisions/0043-pick-the-item-type-first.md) and [0100](decisions/0100-item-types-in-a-module-tree.md).

1. **Choose a type.** A tree on the left lists the [item types](item-types.md) of the build under their modules, as the [Modules window](#modules-window) does. Each module is a bold row, expanded, with its types under it, each with its icon. The built-in module comes first, then the others by name, and each module's types are sorted by name. Types of missing modules are not offered. A pane on the right describes the current row: a type's icon, name and description, or a module's name and description. It shows nothing else about the type. A double click on a type, or Enter, or **Next**, goes on with the selected type. On a module row, Next is disabled and a double click opens or closes the module.
2. **Edit the item.** The chosen type, shown with its icon, then the name, the profile for a type that uses one, the [Watch](#watched-items) check box for a type that reports status, the [keys for the page](#keyboard) for a type whose views are web views, and the configuration. A type uses a profile when its views are web views or its module has settings screens. The profile list starts with "<name> (from the group)", such as "Work (from the group)". The configuration uses the type's own editor if it has one, or else a form built from the type's fields for the profile the item would use, such as GitLab's project paths of that profile. Choosing another profile builds the form again and keeps what was entered. **Back** returns to the type tree. Choosing another type starts from that type's defaults; choosing the same type keeps what was entered. OK is refused while the name is empty or the configuration has problems, and a message under the page says why.

**From a link.** Above the tree, a field says "Paste a link to create an item from it". While it holds an `http` or `https` address, the tree shows only the types that can show it, best first: the types of modules, then Web page. The module of the best match comes first, and the best match is selected, so Enter goes on with it. Choosing a type fills its configuration from the link, and the name too, unless one was typed. Each type reads the link with the server in its module settings, in the profile of the item's group, such as a self-hosted GitLab. The item names no server of its own: it uses that of its profile. A note under the tree says when no type can show the link, or when the text is not a web address; every type is shown then. Clearing the field shows every type again. Which types take which links is listed in [item types](item-types.md#items-from-a-link). See [decision 0049](decisions/0049-items-from-a-pasted-link.md).

**Paste URL….** The context menu of a task and of its Items row has "Paste URL…" after "New item…". It opens the item dialog with the link from the clipboard in the field. It is disabled while the clipboard holds no web address.

"Edit item…" opens the second page only. The type of an existing item cannot change. An item whose module is missing shows its name only, and keeps the rest unchanged.

## Modules

A module can have settings screens for values that belong to the whole module, such as GitLab's server and list of project paths. Each profile has its own values. See [modules](modules.md#settings-screens) and decisions [0044](decisions/0044-module-settings-in-the-workspace.md), [0050](decisions/0050-profiles-hold-logins-connections-and-settings.md) and [0054](decisions/0054-module-settings-replace-connections.md).

- **The menu.** Each profile's submenu of the [Profiles menu](#profiles-menu) has Modules, with a submenu for each module with screens, named after the module. Each lists the module's screens by their names. A module without screens does not appear.
- **The dialog.** Choosing a screen opens its dialog for that profile only, titled with the profile, the module and the screen, such as "Work: GitLab General Configuration". It shows the screen's form, or the module's own editor, and a note that the settings belong to that profile, that items using it read them, that they are saved in the workspace and shared with it, and that tokens stay on this device. A field that has no stored value shows its default as its text, such as GitLab's `https://gitlab.com`, so the value the module uses is visible. OK stores the values and is refused while the form reports a problem. Cancel changes nothing.
- **Lists.** A list field, such as the project paths, takes one entry per line. Blank lines are dropped.
- **Tokens.** A token field, such as GitLab's and Jira's **Token**, shows its text hidden and always starts empty. The token itself is never shown. Its placeholder says whether this device has one: "Set on this device. Type to replace it." or "Not set on this device.". A set token has a **Remove** button, which removes it from this device when OK is pressed. Typing nothing keeps the token. What is typed there is never stored in the workspace; it goes to this device's keychain, under `<workspace id>/<profile id>/<module id>/<field key>`. Deleting a profile removes its tokens from this device. See [decision 0054](decisions/0054-module-settings-replace-connections.md) and [sync](sync.md#secrets).
- **Using the values.** A module offers its settings where they help. For example, the project path field of every GitLab item is an editable list of the paths configured in the item's profile. Any other path can still be typed.

## Modules window

Help > Modules… opens the Modules window. It shows what this build of the app can do: every [module](modules.md) built into it, and the [item types](item-types.md) each one adds. It is read-only. See [decision 0056](decisions/0056-modules-window.md).

- **Opening it.** The window is not modal, like the [settings window](#settings). Choosing Modules… again brings it to the front. Close closes it.
- **The list.** On the left, the built-in module "Built in" first, then the other modules by name, such as GitLab and Jira, all in bold. Under each, its item types by name, with their icons. A module's or type's description is also its tooltip.
- **The details.** On the right, a pane describes the selected row. The first module is selected when the window opens.
  - For a module: its name and description, then its version, author, homepage and UUID, each on its own line and only when the module gave it, then its settings screens, such as "Settings: General Configuration", and where each profile edits them, in Profiles > <profile> > Modules > <module>. A module without screens says "No settings." Then each of its item types, with its icon, name and description.
  - For an item type: its icon, name and description, and the module it belongs to.
- **Descriptions.** Each module and each item type gives its own, in one or two plain sentences. See [what a module registers](modules.md#what-a-module-registers) and [the type registry](item-types.md#the-type-registry).
- **Homepage.** A module's homepage is a link. Clicking it opens the page in the system's browser. See [decision 0089](decisions/0089-module-identity.md).
- **Built-in types.** The core registers its own module, "Built in", with the [built-in panels](item-types.md#built-in-panels). It is part of every build. See [decision 0094](decisions/0094-built-in-panels-as-a-core-item-type.md).

## Settings

The settings window holds settings of this device. They are kept in the [device settings](sync.md#device-settings), not in the workspace, so they are not synced. Module settings, which belong to the workspace, are in the [Profiles menu](#profiles-menu).

- **Opening it.** On macOS it is the standard Settings… item of the application menu, with `Cmd+,`. On Windows and Linux it is Tools > Settings…, with the platform's preferences key if it has one. There is no Edit menu, since the app has no edit commands, and Tools is where Windows apps keep their options. See [menus](#menus).
- **Applying changes.** Each change is saved and applied at once, without a restart, except the language. The window has only a Close button.
- **Layout.** The options are grouped in sections. Each section is a bold heading over a form. A note at the top says that the settings apply to this device only.
- **General:**
  - "Language": "System", the default, "English" or "Português (Brasil)". "System" takes Brazilian Portuguese when the system's first user interface language is any Portuguese, and English otherwise. Next to it, "Applies at the next start.": unlike the other options, the language changes only when Mirante starts again. Dates and numbers follow the language. See [decision 0086](decisions/0086-english-and-brazilian-portuguese.md).
- **Sidebar:**
  - "Highlight the sidebar row of the cell under the mouse", on by default. It turns the hover tint of the [sidebar tree](#sidebar-tree) on or off. Turned off, any tint goes at once.
- **Status:**
  - "Check servers every", a number of minutes from 1 to 60, 2 by default. It sets how often the app asks the servers for the status of the items it watches. See [polling and caching](integrations.md#polling-and-caching).
- **Window:**
  - "Global hotkey", the [keys](#keyboard) that show or hide Mirante from any application. "Clear" turns it off. Below it, a message says if the keys could not be registered.
  - "Closing the window keeps Mirante running in the tray", off by default. See [tray and notifications](#tray-and-notifications). Without a tray it is off and disabled.
  - "Start Mirante when I sign in". Mirante then starts hidden in the [tray](#tray-and-notifications). The system holds this choice, so a change made in its own settings shows here. On macOS, the system may ask the user to allow it in System Settings > General > Login Items, and Mirante offers to open that page. Not shown before macOS 13. See [decision 0106](decisions/0106-start-at-sign-in.md).
- **Terminal:**
  - "Terminal command", the command that opens a terminal, with `{dir}` for the folder and `{command}` for the program to run. Empty, the default, uses the system's terminal; the grey placeholder says so and shows an example. See [terminal launch](integrations.md#terminal-launch).
- **Modules:** a group for each module that declares device settings, titled with the module's name, with one field per setting, such as the `folder` module's editor. Each field shows the stored value, else the module's default. Only a value the user changes is stored; emptying a text field removes it, so the default applies again. Without such modules, as in a build without modules, the section is left out. See [decision 0079](decisions/0079-module-device-settings.md).

## Window

When the window closes, its place and size are saved on this device, and whether it is maximized. The sidebar width is kept too. The place and size are those of the normal window, also when it was maximized or full screen.

At start, the window opens at the saved place and size, maximized if it was. The saved place is used only while it is still visible on a connected screen:

- the lower half of the title bar lies wholly on the screens, so the window can be moved, and
- at least half of the window is on them.

Only the lower half of the title bar counts, because title bars differ in height between systems; a window against the top of a screen keeps its place.

Otherwise, as after a monitor was unplugged or a resolution shrank, the window opens centred on the primary screen, at the saved size made to fit that screen. Without a saved place, as from earlier versions, it opens centred too. The check works on the screens' available areas, without the menu bar and task bar, and is a pure function in `core/WindowPlacement`. The app's icon, a temporary one, is a blue square with three dashboard panes.

### Starting Mirante again

Only one Mirante runs with each [web data folder](README.md#glossary), because two would damage each other's logins. Starting Mirante while it runs brings the running window to the front, restored if it was minimized or hidden in the tray, and the new start exits. A workspace given with `--data` opens in the running window, unless it is the one already open, or a dialog is open there: then the window only comes to the front, with the dialog. If the running Mirante does not answer within five seconds, the new start says that Mirante is already running with this web data folder. Test runs that must run beside the user's own Mirante use `--web-data <folder>` for their own logins. See [decision 0058](decisions/0058-one-instance-per-web-data-folder.md).

## Look

Standard controls, such as buttons, menus, lists and dialogs, keep the platform's own look. Mirante draws a few controls itself, in the same way on every platform, so the app looks calm on macOS as well as on Windows and Linux. See [decision 0041](decisions/0041-own-look-for-tabs-and-tool-buttons.md).

- **Tool buttons** outside a toolbar, such as the sidebar buttons, the cell header buttons and the web toolbar, are flat. A light rounded fill shows when the mouse is over one, and a stronger one while it is pressed or on.
- **Tabs**, in the root tab bar, in the Focus and preview slots, in tab groups and at the foot of the sidebar, are rounded fills. The current tab is filled with the highlight colour, the same colour as the sidebar's selected row, so it is easy to find. Its text, icon and close button take the highlighted text colour, which is readable on the highlight in light and dark palettes. When a tab of the other bar in the tab row is current, the bar shows no current tab. The tab under the mouse has a light grey fill. See [decision 0065](decisions/0065-current-tab-in-the-highlight-colour.md).
- **Close buttons on tabs** are on the right. They show on the current tab and on the tab under the mouse only.
- **Cell borders** are a thin, light line with rounded corners.
- **The warning bar** has a tinted background and no frame.
- **Push buttons** show no focus ring. On macOS the ring is drawn outside the button, and a dialog's layout cut it into stray bars around the default button. The default button still shows by its colour.
- **Dialogs on macOS** are attached to the main window or the detached window they belong to, as child windows. They stay directly above that window and come forward with it, so no other app's window can come between them. A dialog opened from another dialog follows its parent dialog. On Windows and Linux nothing changes. Sheets, which slide down from the title bar, were tried first, but Qt decides whether a dialog is a sheet before an app-wide hook can see that it is modal. The code is in `src/platform/AttachedDialogs.cpp` and `AttachedDialogsMac.mm`.
- **Forms** in dialogs fill the dialog's width and start at its top left on every platform. On macOS, Qt would otherwise keep each field at its natural width and centre the form, which cuts long values, such as addresses, short.
- **The sidebar** has a light grey background, a little darker than the window, with rounded row shapes, as in macOS sidebars. See [sidebar tree](#sidebar-tree).
- **Header tints.** Cell headers and title bars have a light tint, mixed into the window colour, so text stays readable in light and dark palettes. The title bars of task dashboard and item previews, and task cell headers, have a 22% tint of the task's [group colour](data-model.md#group). Item cell headers have a lighter 12% tint of the group colour of the item's task, in every dashboard, so an item placed on a root dashboard shows its group too. Empty and removed cells keep the highlight colour. See [decision 0046](decisions/0046-group-colours.md) and [decision 0047](decisions/0047-item-headers-in-group-colours.md).

## Menus

The menu bar has File, View, Profiles, Tools and Help. On macOS there is no Tools menu: its one entry, Settings…, is in the application menu.

- **File:** what makes things comes first, then the workspaces, then Quit:
  - New Task from Link… (`Ctrl+Shift+N`) and New Group…;
  - New Workspace… (`Ctrl+N`), Open Workspace… (`Ctrl+O`), Open Recent with the last ten workspaces and Clear Recent, and the **Workspace** submenu for the open workspace: Rename…, Move…, Show Folder and, after a separator, Reload (`F5`);
  - Quit (`Ctrl+Q`).

  A workspace is a data folder with a `workspace.mirante` file at its top. Open Workspace… chooses that file. New Workspace… chooses a folder and a name and writes the file. A new workspace starts with one empty root dashboard, "Main", open as a tab with editing on, so the content area is not empty. It is an ordinary root dashboard: it can be renamed, closed or deleted. Opening an existing workspace never adds one. A folder without the file is not opened; from the menus the app offers to make it a workspace. At start the app opens the workspace given with `--data`, otherwise the last one used on this device if it still has its file, otherwise the sample data. The window title shows the workspace's name: the name in its `workspace.mirante`, else the folder's name. The sidebar header does not repeat it, so its buttons keep their room. File > Workspace > Rename… renames it, and the Store writes `workspace.mirante`. The sample data cannot be renamed. File > Workspace > Move… moves the workspace's folder to an empty folder, or into a new folder with the same name inside a folder that is not empty, and opens it there. The device settings that name the folder, such as the recent list and the open tabs, follow it. Logins and tokens are kept by the workspace's id, which moves with it, so they keep working. A folder that another program holds open is not moved, and the warning bar says so; see [decision 0055](decisions/0055-move-workspace.md). File > Workspace > Show Folder opens the workspace's folder in Explorer, Finder or the desktop's file manager; it is disabled for the sample data, which has no folder. File > Workspace > Reload opens the workspace again from its files. See decisions [0025](decisions/0025-workspace-file.md) and [0027](decisions/0027-workspace-needs-its-file.md).
- **View:** Sidebar (show or hide, `Ctrl+B`), Focus Sidebar (`Ctrl+Shift+E`; shows the sidebar and puts the keyboard focus in its tree, on its selected row or else its first), Expand All Tasks, Collapse All Tasks, Full Screen (`F11`, `Ctrl+Cmd+F` on macOS; leaving it brings back the maximized or normal window, and a window closed in full screen opens as it was before; in full screen the menu bar hides and shows while the mouse is at the top edge or a menu is open, and every shortcut keeps working, while macOS shows its own menu bar), Go To… (`Ctrl+K`, the [quick switcher](#keyboard)), All Dashboards… (`Ctrl+D`), and New Dashboard (`Ctrl+Shift+D`).
- **Profiles:** Manage Profiles…, then a submenu for each profile with its Modules submenu. See [Profiles menu](#profiles-menu).
- **Tools:** Settings…, which opens the [settings window](#settings). Windows and Linux only.
- **Help:** Modules…, which opens the [Modules window](#modules-window), and Open Sample Data, which opens the read-only [sample data](README.md#glossary). After a separator, About Mirante, with the version, the author and the project's address, and About Qt. Debug builds, and release builds started with `--log`, also have Show Log (`Ctrl+Shift+L`), a window that lists the app's Qt log messages as they arrive, including the JavaScript console of web pages, with a level and a text filter. Chromium's own messages go to the console only.

## Opening tasks

- **Single click:** selects a row. It opens nothing.
- **Double click** (or `Enter`) on a task: expands or collapses it. Tasks are opened through their dashboards.
- **Double click** on a task dashboard: opens it in the **preview slot**, right of the tabs. The slot shows the dashboard's icon and its title in italics, with a close button. The next dashboard opened this way replaces it. Every preview opens there: task dashboards and items from the sidebar, "Open Alone", the quick switcher, Focus and new task dashboards. With no preview, the slot is hidden. A middle click on the slot closes it, and its context menu holds "Close preview". Task dashboard titles name the task and the dashboard: "Task / Dashboard". The title bar below the tab shows them in pills, as cell headers do: the task in bold, the dashboard in a grey pill with the dashboard icon, and the task's project in a grey pill. An item preview's title bar shows the item in a pill with its type's icon and colour instead of the dashboard. A task cell's header uses the same pills. The title bar and the task cell header are tinted with the task's group colour, so they show which group the task is from. The tint follows at once when the group's colour changes or the task moves to another group. The slot's tooltip reads "Project / Task / Dashboard".
- **Double click** on an item: opens the item alone in the preview slot, like a task dashboard, titled "Task / Item" with the item type's icon. It is read-only. See [decision 0023](decisions/0023-item-tabs.md).
- **No pinning:** only one preview exists, and it never becomes a tab. Editing the previewed dashboard's layout keeps it in the preview slot. To keep a task dashboard or an item in view next to others, place it on a root dashboard, for example with "Open in new root dashboard". See [decision 0052](decisions/0052-no-pinning.md).
- A task dashboard preview has a title bar with the task and dashboard pills and its own lock. The lock at the end of the tab row is for root dashboards, and hides while a task dashboard preview is in front. See [decision 0022](decisions/0022-task-dashboard-tabs.md).
- When the preview comes to the front, the sidebar selects its dashboard or item under its task.
- Opening the task dashboard or the item that the preview already shows switches to the preview.
- Closing the last tab, or closing the preview while there is no tab, shows Focus.

The open tabs are stored on each device for each workspace, not synced: the root dashboard tabs in the order they have in the tab row, the current tab, and the [detached](#detached-windows) root dashboards. If the Focus slot was current, it is current next time. The preview is not kept; if it was current, the first tab is current next time. They are saved when another workspace opens and when the window closes, and come back when the workspace opens again. The first time a workspace opens on a device, every root dashboard opens.

## Root dashboards

A root dashboard is a tab in the main tab bar. It has a name and a [layout tree](data-model.md#layout-tree).

- A cell can hold a task, showing one of the task's dashboards, or a single item from any task.
- A task dashboard inside a root dashboard shows a thin header with the task and dashboard names in bold, such as "Rotate TLS certificates / Main", its project in a grey pill after them, such as "infra", and a button to open it alone. The grey pill is the same one that item cells use for where an item comes from. It has no picker: to show another dashboard of the task, drop that dashboard from the sidebar.
- The `+` button creates an empty root dashboard, with editing on.
- A new workspace starts with one empty root dashboard, "Main". See [menus](#menus).
- Closing a root dashboard's tab, with its close button, a middle click or the tab bar's context menu, does not delete it. It can be opened again with the [quick switcher](#keyboard) or the dashboard list.
- The **dashboard list**, opened with the button after the lock in the tab bar row or with "All dashboards…" in the tab bar's context menu, lists every root dashboard and marks closed ones. It opens a dashboard again, and it is the only place where a root dashboard is deleted, after a confirmation.
- Right-clicking the tab bar shows "New dashboard", "Close tab" over a tab, "Rename dashboard…" and "Detach into Window" over a root dashboard tab, and "All dashboards…". "Rename dashboard…" opens the dashboard dialog with the name to edit.

### Placing tasks and items

- Unlock the dashboard, then drag an item or a task dashboard from the sidebar onto it. A task dashboard dropped on a root dashboard places its task there, showing that dashboard. The drop overlay shows where it lands. See [editing](#editing-layouts). A locked dashboard refuses every drop: it shows no drop zones, and the cursor shows that the drop is not possible.
- Or use "Add to dashboard…" in the context menu, then pick a dashboard and click a cell. An empty cell is filled; any other cell is split.
- Tasks go only into root dashboards. A task dashboard shown in a root dashboard is read-only there: it has no lock, no edit menus, and takes no drops. A drop on it acts on the root dashboard. To change a task dashboard, open it on its own with a double click on its cell, its "Edit task dashboard" button or its menu, and edit it there. See [decision 0020](decisions/0020-edit-task-dashboards-alone.md).

## Task dashboards

A task dashboard has the same layout engine as a root dashboard. Its cells hold items of the task.

Each cell has a small header containing:

- a **type pill**: a small rounded label with the type's icon and short name, such as "MR", "Issue", "Jira", "Claude", "Folder", "Image", "Notes" or "Web". Its colour depends on the module, so items of one module look alike. A type whose module is not in the build gets a grey pill with the module id;
- the item name, in normal weight;
- in a root dashboard, where an item can come from any task, a grey **place pill** with its project and task after the name, such as "backend › Overview";
- the item's [status](#attention): an attention badge, a problem pill, or the data's age;
- buttons: refresh, open in browser, a menu, and maximise. Refresh shows only for items of types that report status.

### A bigger view of an item

Some work, such as signing in to a site, is easier on a large page than in a small cell. An item cell offers two bigger views. Both work on locked dashboards, and in task dashboards shown inside root dashboards, because neither changes a layout. Empty and "Removed" cells, and task cells, offer neither. See the [M4 plan](plans/m4-web-views.md#part-6-a-bigger-view-of-an-item).

- **Maximise.** The maximise button in the cell header, the cell menu entry "Maximise", or `Ctrl+Shift+M` for the focused cell, shows the cell over the whole dashboard area and gives it the focus. The button's tooltip names the shortcut. The same cell moves, so a web page keeps its state, including a login in progress. The button, the menu entry "Restore" or the shortcut again puts the cell back in its place. Maximising another cell puts the first one back. Changing the root tab or closing the tab also puts it back.
- **Open Alone.** The cell menu entry "Open Alone" opens the item alone in the preview slot, as a double click on the item in the sidebar does. See [decision 0023](decisions/0023-item-tabs.md). The preview has a view of its own, in the same web profile, so a login made there reaches the cell when its page reloads. An item preview already shows its item alone, so it does not offer "Open Alone". Every item cell also has an "Open Alone" button, last in its header, as a task cell has its own. The cell menu button (⋮) shows only while the dashboard is unlocked; locked, these buttons cover its view commands, and a right click on the cell still opens the menu.
- **Changing the item from its cell.** The menu of an item cell also offers two entries that change the item, not the layout, so they work on a locked dashboard and in an item preview too:
  - "Edit Item…" opens the item dialog.
  - "Watch", checked while the item is [watched](#watched-items), for a type that reports status. It turns watching on or off.
  - "Use Current Address" is there for a cell that shows a web page, when the item's type has an address field. It saves the address the page shows now in that field, such as the Address of a Web page item. It is disabled when the two are already the same, and its tooltip shows the address.

On a locked dashboard the cell menu of an item cell holds only these entries. On an unlocked one they come first, before the [editing commands](#editing-layouts).

### Layout presets

"New dashboard" and "Apply preset" offer starting layouts. A task dashboard made with "New dashboard…" in the sidebar opens in the preview slot.

- Full: one cell.
- Two side by side.
- Two stacked.
- Grid of four.
- One large on top, two below.
- One large on the left, two stacked on the right.
- Tabs: one tab group with one cell per item.

A preset only creates the first tree. The user can change it freely afterwards.

## Editing layouts

Layouts are locked by default, to prevent accidental changes while working. The lock button at the far right of the tab bar row turns editing on for the current tab. Task dashboards shown inside a root dashboard have no lock of their own; they are edited on their own tab. Resizing splits works even when locked.

`F2` locks or unlocks the current dashboard, as a click on its lock button does. For a root dashboard that is the lock at the end of the tab bar row. For a task dashboard preview it is the lock in its title bar. Focus and an item shown alone have no lock, so `F2` does nothing there. The lock buttons name the key in their tooltips. `F2` works while the sidebar's tree has the keyboard focus too.

When editing is on:

- **The cell context menu** offers, after "Maximise" and "Open Alone" for an item cell (see [a bigger view](#a-bigger-view-of-an-item)):
  - split right;
  - split down;
  - wrap in tabs;
  - add tab;
  - clear cell;
  - remove cell.

  Clear cell, remove cell, and deleting a tab of a tab group ask for confirmation, since they remove content from the layout.
- **The tab context menu** offers add tab (an empty tab after the clicked one), rename, "Delete tab", move to the left or right, and "Ungroup".
  - "Delete tab" removes the tab and its content from the layout.
  - "Ungroup" appears only when the group has one tab. It removes the tab group: the tab's content takes the group's place.
- **A `+` button** at the right of each tab group adds an empty tab at the end.
- **Moving a tab:** dragging a tab along its tab bar moves it within its tab group, and the group's order is saved with the layout. While the dashboard is locked, the tabs stay in place.
- **Drag and drop:** drag a cell by its header, a tab by its tab out of its tab bar, a whole tab group by the empty part of its tab bar, or an item from the sidebar. Cell headers and tab bars show an open hand while the dashboard is unlocked. Holding a drag over a tab that is not shown opens it, so a cell can be dropped beside its content. A tab group or a split moves within its own dashboard. The zones work over web pages too: a web page does not take a drag of a cell or an item, but it still takes other drags, such as files. A semi-transparent overlay shows five drop zones on the cell under the cursor:
  - the left, right, top or bottom edge splits the cell on that side;
  - the centre adds the dropped content as a tab, turning the cell into a tab group if needed. On an empty cell, or a "Removed" one whose task or item was deleted, the centre fills the cell instead.
- Dropping on a tab bar inserts a new tab at that position.
- **Replace:** holding `Ctrl` while dragging makes the drop replace the content of the cell under the cursor, instead of splitting it or adding a tab. The whole cell is highlighted. A moved cell leaves its old place.

After each edit, the tree is [normalised](data-model.md#normalisation).

## Pasting images

A pasted image becomes an item of a task. It is never added straight to a dashboard. See [decision 0051](decisions/0051-paste-images-from-the-items-menu.md).

- **Paste Image…**, in the context menu of a task's Items row or of the task row, adds the clipboard's image to that task as a new Image item. It is enabled only while the clipboard holds an image. No dashboard changes: the item shows in the sidebar, and the user drags it onto a dashboard if wanted.
- `Ctrl+V` replaces the image of the Image item in the focused cell, or in the item preview when it is current. It adds nothing. Anywhere else on a dashboard, the status bar says "To add an image, use Paste Image… in a task's Items menu."
- The "Paste" button of an image field in the item dialog fills that field.

An image can also be dropped from another app, such as an image file from the file manager or an image from a web page in a browser. See [decision 0085](decisions/0085-drop-images-from-other-apps.md).

- A drop on a task row, or on its Items row, in the sidebar does what **Paste Image…** does there. The row under the image is tinted while it is dragged.
- A drop on a cell that shows an Image item does what `Ctrl+V` does in that cell: it replaces the image, on a locked dashboard too. The cell shows its focus border while the image is over it.
- A drop on an image field in the item dialog does what its "Paste" button does.

Any other local file dropped on a task row, or on its Items row, becomes an item of the first type that takes it, named after the file, on no dashboard. With the Documents module, a Markdown, text or PDF file inside one of the project's folders becomes a Document (project file), and one anywhere else a Document (workspace file), copied into the workspace: above 20 MB it asks first, above 50 MB it is refused, and the status bar says "<name> was copied into the workspace." A file no type takes says "No item type can show this file." in the status bar. See [decision 0104](decisions/0104-document-items.md).
- The drop takes the first file whose suffix is `png`, `jpg`, `jpeg`, `gif`, `bmp` or `webp` and that can be read, else the image the drag carries.
- Anywhere else, and for a drag without such an image, such as a text file, the pointer shows that the drop is refused. The Trash list refuses every drop.
- A drop is always a copy: the file stays where it was. The dialog opens once the drop is over, so the app the image came from does not wait for it.
- The sidebar's own drags, and the drags of the layout editor, work as before, also over an Image cell.

Each of these, drops included, opens the paste dialog first. What the paste then does is described under [image](item-types.md#image). See [decision 0045](decisions/0045-preview-and-crop-pasted-images.md).

- The dialog, "Paste Image", shows the image scaled to fit. A small image keeps its own size. Under it, a line gives the image's size in pixels, such as "1920 × 1080 pixels".
- **Cropping.** Dragging on the image draws a crop rectangle. The image outside it is dimmed. The line under the image then gives both sizes, such as "Crop: 800 × 600 of 1920 × 1080 pixels".
  - Handles on the corners and the middle of the edges resize the crop. Dragging inside it moves it. The mouse pointer shows which one is under it.
  - The crop stays within the image and is at least one pixel wide and high. An edge dragged past the opposite edge turns the rectangle over.
  - Dragging outside the crop draws a new one. A click without a drag removes it.
  - **Whole Image** removes the crop. It is enabled only while there is a crop.
- The crop is in the image's own pixels. The pasted image is cut at full resolution, not at the size of the preview.
- **Paste**, or `Enter`, pastes the cropped image, or the whole image without a crop. **Cancel**, or `Escape`, pastes nothing.
- The dialog opens only when the paste can happen. In the read-only sample data, in a build without an image type, or for `Ctrl+V` without a focused Image item, the status bar gives the reason at once and no dialog opens.
- The preview has a light rounded backdrop, drawn with the text colour mostly transparent, as the [flat controls](#look). The crop border and handles use the palette's highlight colour. Both suit light and dark palettes.

## Detached windows

A root dashboard tab can be detached into its own operating system window. Nothing else is detached: not cells, and not the preview. See [decision 0024](decisions/0024-detach-root-dashboard-tabs.md).

- "Detach into Window" in the context menu of a root dashboard tab takes the tab out of the tab bar and moves its dashboard into a new window. The dashboard is moved, not made again, so its web pages keep their state. Only cells whose item type cannot move to another window are made again.
- The window's title is the dashboard's name. A rename changes it.
- A small toolbar holds "Put Back" and the dashboard's own lock. The lock works as the lock of the main window does. `F2` in the window locks or unlocks its dashboard.
- Closing the window, or "Put Back", puts the tab back at the end of the tabs and makes it current.
- Opening a detached root dashboard from the sidebar, the quick switcher or the dashboard list brings its window to the front instead of opening a tab.
- Deleting a root dashboard closes its window.
- The window's position and size are stored on each device when it closes. A new window opens at the stored position and size, or at a default size.
- The detached root dashboards are saved with the [open tabs](#opening-tasks). When the workspace opens again, they open again in their windows.
- Opening another workspace closes the detached windows, after saving them. Closing the main window puts them back, after saving them, so they open detached next time.
- The keyboard shortcuts of the main window, such as moving the focus between cells and maximising a cell, work in the main window only. The lock key, `F2`, is the exception: each window has its own.
- The maximise button and the cell menu entries "Maximise" and "Restore" work in a detached window too. The cell is maximised over the window's dashboard, below its toolbar, and stays in that window. Changing the main window's tab leaves it maximised. "Open Alone" opens the item in the main window's preview slot and brings the main window to the front.

If the same item is shown in two places, each place has its own live view. See [decision 0003](decisions/0003-view-per-placement.md).

## Focus view

Focus answers "what should I work on now?", across all projects.

- Focus has a fixed slot right of the tabs, before the preview slot. A click on the slot selects it, and so do `Ctrl+PgDown` and `Ctrl+PgUp` on their way through the tab row. The slot carries a target icon, since Focus is a list and not a dashboard. The sidebar has no Focus entry; see [decision 0053](decisions/0053-no-focus-entry-in-the-sidebar.md). The slot is always there and cannot be closed. See [decision 0048](decisions/0048-focus-and-preview-slots.md).
- It lists active tasks from all projects, or from one group, chosen in a combo box. Tasks in the [trash](#trash), and the tasks of projects in the trash, are left out. The combo box holds "All groups", then the groups in manual order.
- Each row shows the task title, then its project and group, dimmed, and the [priority](data-model.md#priority) marker of the sidebar, a circle as for every task. Low tasks are dimmed.
- A right click on a task opens the task's menu from the sidebar: edit, set priority, set state, delete and the rest.
- **Sort order:**
  1. task priority;
  2. project priority, to break ties;
  3. manual order.
- The list follows changes to groups, projects and tasks as they happen.
- A double click or `Enter` opens the task's default dashboard in the preview slot.
- Each row shows the task's [attention badge](#attention) at its right.
- A check box right of the group combo box, "Needs attention first", moves the tasks with attention badges to the top. Within each part the usual order stays. The box is off at first, and its state is kept in the [device settings](sync.md#device-settings).

## Attention

Attention means something needs action now. Sources:

- a pipeline failed;
- someone requested your review;
- your merge request was approved, or has conflicts;
- a linked Jira issue changed status.

Attention appears as badges. It changes the order only in the Focus view, and only when its toggle is on. See [decision 0009](decisions/0009-priority-and-attention.md).

Some reasons are events, such as an approval or a new Jira status. They stop counting once a cell showing the item has been visible for two seconds. The others, such as a failed pipeline, count for as long as they are true. See [decision 0061](decisions/0061-attention-from-status-of-any-rendering.md).

- **Badges.** A badge is a small round label in the attention colour, a strong red, with a white count. Counts above 99 show as "99+".
  - In a **cell header**, before the refresh and menu buttons, it counts the item's reasons. Its tooltip lists them.
  - In the **sidebar**, at the right of the row, it counts the task's items that need attention. A project's badge sums its tasks, and a group's badge sums its projects. Only active tasks count, so done and archived tasks and anything in the trash count zero. The row's tooltip gives the count too.
  - In **Focus**, each row has the task's badge, as in the sidebar.
- **Problems.** When the item's status cannot be fetched, the cell header shows an amber pill in place of the badge: "Token not set", "Token refused", "No server set", "Not found", or "Error" for a network or server error with no older data. For the first three the pill is a button that opens the profile's settings screen of the module. Its tooltip gives the message and the profile's name.
- **Old data.** When the last fetch failed with a network or server error but older data exists, the header shows the data's age in grey, such as "12 min ago". The badges keep the older data's count.
- **Refresh.** The refresh button in the cell header fetches the item's status at once. Otherwise the app checks the servers at the interval set in the [settings window](#settings).

### Watched items

Attention can also reach the user outside the app, through the tray icon and desktop notifications. Only **watched** items do this. See [decision 0087](decisions/0087-watched-items.md).

- A watched item that needs attention counts on the [tray icon](#tray-and-notifications), and a failed pipeline in it sends a desktop notification. An item that is not watched does neither.
- Items start not watched, also those made before watching existed.
- Watching changes nothing else: the badges above show every item's attention.
- Watching is offered for types that report status. The item dialog has a "Watch" row with "Count on the tray icon, and notify". The context menus of the item in the sidebar and in a cell header have a checkable "Watch".
- The choice is stored in the item, so every device of the workspace watches the same items.
- A watched item of an active task is polled even when nothing shows it, so a failure is seen while the window is hidden.

## Quick add

Quick add creates a task from a link, or adds items to a task from a link. It has two entries, named after what they do (see [decision 0083](decisions/0083-quick-add-into-a-task.md)):

- **New Task from Link…**, in the File menu, as a link icon button in the sidebar header, and with `Ctrl+Shift+N`, creates a task. The dialog's title is "New Task from Link".
- **Add Items from Link…**, in the context menu of a task and of its Items row in the sidebar, adds items to that task. The dialog's title is "Add Items from Link: <task title>".

Open either, then paste a link. What it recognises depends on the [modules](modules.md#quick-add) in the build. With the `gitlab` and `jira` modules:

- **A Jira key or issue URL:**
  - The task is named after the issue summary.
  - A Jira Issue item is added.
  - The app searches for merge requests whose branch or title contains the key, and offers to add them.
- **A GitLab merge request URL:**
  - The task is named after the merge request title.
  - An MR summary item and a CI Pipeline item for its branch are added.
  - A Jira key found in the title or branch adds the issue too.

The project is chosen by matching the merge request's GitLab project or the issue's Jira project key against the projects' module settings. If no project matches, or several do, the app asks.

The dialog for a new task, from top to bottom (see [decision 0078](decisions/0078-quick-add-dialog.md)):

- **Text.** When the dialog opens with one line of text on the clipboard, that text is filled in and read at once. Otherwise the app reads the text when it is pasted or when the user presses Return. Typing alone does not start a reading.
- **Profile.** The modules read the text with the servers of this profile. It starts as the profile of the group of the entry selected in the sidebar, else the workspace's default. Choosing another profile reads the text again. The new items still use their group's profile.
- **Status.** "Reading…" while the modules read the text, "Mirante does not recognise this text." when no module takes it, and "Looking for related items…" while related items are searched.
- **Read as.** Shown only when several modules take the text. It lists them by module name, and choosing one shows its reading.
- **Title.** The proposed task title, which the user can change.
- **Project.** Chosen when exactly one project matches. Otherwise it is empty, and the user must choose one. A line below says "No project matches acme/backend" or "Several projects match". Projects in the trash are not offered.
- **Items.** Each item has a check box, its type's icon and its name. Related items join the list, checked, under "Related", as the modules find them. Choosing another project searches for related items again.
- **Create Task** makes an active task of Normal priority in the project, with the checked items in order. Its dashboard has one cell: a single item fills it, and several items are tabs of it, the first one current. The task opens in the preview slot. **Cancel** stops the readings and searches still running.

Without any project, the dialog says "Create a project first." and has no Create Task button. A module that does not answer in 30 seconds counts as not taking the text.

The dialog for a task differs:

- It has no Title and no Project. The project is the task's, so related items are searched for as soon as a reading is shown.
- The profile starts as the profile of the task's group.
- Items whose type and configuration the task already has are not offered.
- **Add Items** adds the checked items to the end of the task's items, in order, as "Add to task as item" does. No dashboard changes, as for [pasted images](#pasting-images): the items show in the sidebar, and the user drags them onto a dashboard if wanted. The sidebar then shows the task; the open tab stays.

## Keyboard

| Key | Action |
|---|---|
| `Ctrl+K` | Quick switcher: fuzzy search over groups, projects, tasks, items and dashboards. Entries in the [trash](#trash) are left out. |
| `Ctrl+Shift+N` | New Task from Link… ([quick add](#quick-add)) |
| `Ctrl+B` | Show or hide the sidebar |
| `Ctrl+Shift+E` | Focus the sidebar's tree, showing the sidebar if it is hidden |
| `Ctrl+D` | All Dashboards… |
| `Ctrl+Shift+D` | New Dashboard |
| `Ctrl+P` | Manage Profiles… |
| `Ctrl+Alt+Arrow` | Move focus to the cell in that direction |
| `Ctrl+Tab`, `Ctrl+Shift+Tab` | Next or previous tab in the focused tab group |
| `Ctrl+PgDown`, `Ctrl+PgUp` | Next or previous tab of the tab row: the tabs in order, then Focus, then the preview, wrapping around |
| `F11` (macOS: `Ctrl+Cmd+F`) | Full screen on or off, as in web browsers. The platform's standard key, `QKeySequence::FullScreen`. |
| `Cmd+,` (macOS) | Open the [settings window](#settings). Windows and Linux use Tools > Settings…. |
| `Ctrl+Shift+M` | Maximise the focused cell, or restore the maximised one. See [a bigger view](#a-bigger-view-of-an-item). |
| `F2` | Lock or unlock the current dashboard, as its lock button does. In a detached window, that window's dashboard. Nothing on Focus or an item shown alone. See [editing layouts](#editing-layouts). |
| `Ctrl+W` | Close the preview or the current tab. Focus cannot be closed. |
| `Ctrl+V` | With an image on the clipboard: replace the image of the Image item in the focused cell or the current item preview, after the [paste dialog](#pasting-images). It never adds an item to a dashboard; a new image item comes from "Paste Image…" in a task's Items menu. See [image](item-types.md#image). A text field keeps its own paste. |
| Global hotkey (`Alt+Shift+D`, on macOS `Control+Shift+D`; set in the [settings window](#settings)) | From any application: hide Mirante if it is in front, else bring it to the front. An open modal dialog only comes to the front. |

On macOS, `Cmd` replaces `Ctrl`.

App shortcuts also work while a web page has the keyboard focus. See [decision 0088](decisions/0088-keys-a-page-keeps.md).

- **Editing keys** stay with a page's text field, as Qt WebEngine decides: copy, cut, paste, undo, redo, select all, and `Ctrl+B`, which is bold in web editors. So in a comment box of GitLab or Jira `Ctrl+B` makes text bold, and anywhere else on the page it shows or hides the sidebar. `Ctrl+V` in a page pastes into the page; it never replaces an image item's image from there.
- **Keys for the page.** An item whose views are web pages can give keys to its page, such as `Ctrl+K` for a web app's own search. The item dialog has a "Keys for the page" row: a list of keys, a field that records a key when it is pressed, **Add** and **Remove**. While a view of the item has the keyboard focus, those keys go to the page and no app shortcut runs. The menus still offer the command. The list is stored in the item as `pageKeys`, and synced.

The global hotkey uses the QHotkey library. It works on Windows, macOS and X11, but not on Wayland, where the user binds a desktop shortcut instead. If it cannot be registered, as on Wayland or when another application holds the keys, the status bar and the settings window say so.

## Tray and notifications

- Closing the main window quits Mirante. A device setting, off by default, makes closing hide the window to the system tray, the menu bar on macOS, instead. The tray icon and the global hotkey work either way: the hotkey hides the app to the tray even with the setting off. See decisions [0075](decisions/0075-hide-to-tray-and-show-hide-hotkey.md) and [0107](decisions/0107-closing-quits-by-default.md).
- While the app is hidden to the tray, polling and notifications continue, and the web pages are frozen: they run no scripts and load nothing until the app shows again. See the [memory policy](web-views.md#memory-policy) and [decision 0108](decisions/0108-freeze-web-pages-in-the-tray.md).
- Hiding also hides the other open windows, such as detached dashboards and the settings window. They come back with the main window, and detached dashboards stay detached. The window's place and the open tabs are saved each time it hides.
- On macOS Mirante leaves the Dock while it is hidden.
- Mirante can start when the user signs in, set in the [settings window](#settings) or on the last page of the Windows setup. It then starts hidden in the tray, as `--hidden` does, until the tray, the global hotkey or a second start calls it. If no tray appears within the minute Mirante waits for one, the window shows minimized. The Windows setup's last page also has "Run Mirante". See [decision 0106](decisions/0106-start-at-sign-in.md).
- On macOS the Dock shows the bundle's icon file, drawn on the macOS icon grid. See [decision 0093](decisions/0093-dock-shows-the-bundle-icon.md).
- The macOS menu bar shows a monochrome icon, `resources/icons/mirante-tray.svg`, as a template image. macOS draws it in the menu bar's colour, like other apps' icons, with the M and the sun in full colour and the edge of the ground as a lighter line. It is the app icon of [decision 0097](decisions/0097-app-icon-monte.md) without its background. Windows and Linux show the app icon.
- Clicking the tray icon brings Mirante to the front. On macOS, a click opens the menu instead.
- The tray menu has "Show Mirante", "Settings…" and "Quit". File > Quit also exits. Under an open modal dialog, "Quit" brings the dialog to the front instead.
- The tray icon shows the number of tasks that need attention through a [watched item](#watched-items): active tasks, not in the trash, with at least one watched item that needs attention. A task with two such items counts once. On Windows and Linux the number is a red badge with a white rim at the icon's lower right, "99+" above 99. On macOS it is the status item's title, next to the icon, so it follows the menu bar's light or dark look. With no such task the icon is plain. The tooltip says "Mirante: 2 tasks need attention", or "Mirante" alone.
- If the platform has no system tray, for example GNOME without an extension, closing the window quits the app, and the global hotkey minimizes the window instead of hiding it. At start, Mirante looks for a tray for one minute, because at sign-in the taskbar may not be there yet.
- A desktop notification appears only when a pipeline of a [watched item](#watched-items) fails: the modules mark those reasons to notify. Its title is the reason, such as "Pipeline failed", and its text names the task and the item, such as "Rotate TLS certificates / Pipeline". Clicking it brings Mirante to the front and opens the task's default dashboard in the preview. A task deleted or moved to the trash since then only brings Mirante to the front, and an open modal dialog stays in front.
  - Each reason notifies once. The keys of the reasons already notified are kept on the device, so a restart sends nothing again. A pipeline that fails again later is a new reason and notifies again.
  - What is already true when an item is first fetched, or when it starts to be watched, does not notify.
  - When more than three notifications are due at once, one notification says "4 watched items need attention", and clicking it only brings Mirante to the front.
  - Windows and Linux show the tray icon's message. The system shows one at a time, and some Linux desktops do not report the click. macOS uses its notification centre, which asks for permission the first time. Without a tray there are no notifications.
