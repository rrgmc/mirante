# M4 code plan: web views

This plan implements [milestone M4](../roadmap.md#m4-web-views). After M4, web items show live pages: a Web page, a Claude session on claude.ai, and the GitLab and Jira page types. Each module logs in once in each group, in a profile of its own. Root dashboard tabs can be detached into windows.

Native item types keep their M2 placeholders until M6 and M7. There are still no API calls and no tokens.

## Changes to the architecture

- **Qt WebEngine joins the build.** `find_package` adds `WebEngineWidgets`, `WebChannel` and `Network`. `tools/setup-qt.ps1` and `tools/setup-qt.sh` install the `qtwebengine`, `qtwebchannel` and `qtpositioning` add-on modules. CI caches the larger Qt, installs the Linux packages WebEngine needs at run time, and runs the tests with `QT_QPA_PLATFORM=offscreen`. On Linux it also sets `QTWEBENGINE_DISABLE_SANDBOX=1`, because Ubuntu 24.04 runners block the user namespaces Chromium's sandbox needs.
- **The core makes web views; modules may take the profile instead.** A module's `createView` normally asks its `ItemContext` for a web view of a URL, and gets a plain `QWidget`. The core chooses the profile, and handles popups, permissions, downloads and the toolbar. A module that needs full control of its page can take the profile with `profile(url)` instead, and links Qt WebEngine itself. All five web types of M4 use `createWebView`. See [decision 0037](../decisions/0037-core-makes-web-views.md).
- **One profile per group and module.** A module's web items in a group share one profile, and no other module's items use it. Module code that takes the profile reaches only its own module's sessions. See [decision 0040](../decisions/0040-profiles-per-group-and-module.md).
- **Connections are read in M4, without tokens.** The GitLab and Jira page types need the base URL of their connection. M4 adds the [resolution order](../data-model.md#connection) and a minimal connection list in the group dialog: kind, name, base URL and the group default. Tokens, kind-specific fields, testing a connection and project overrides in the project dialog stay in M5. See [decision 0038](../decisions/0038-read-connections-in-m4.md).
- **A new library, `src/web/`.** It holds `ProfileManager` and `WebPanel`, and links Qt WebEngine. `items` links `web`, as the [architecture](../architecture.md#source-folders) already shows.
- **Placements arrive.** A `Placement` is the body of a `CellFrame`. It creates the item's view when it first becomes visible, and reports visibility to the `ViewPool`.
- **`DashboardPage` leaves `MainWindow.cpp`.** The page class moves to `ui/DashboardPage.h`, unchanged, so a detached window can hold one. This keeps `MainWindow.cpp` from growing again.

[architecture.md](../architecture.md) gets the new classes in the commit of the task that adds each one.

## Part 1: the module interface

**`ItemType`** gains four methods:

```cpp
enum class Rendering { Native, Web };
virtual Rendering rendering() const { return Rendering::Native; }

// The connection kinds the type can use, in order of preference, such as
// {"jira.cloud", "jira.dc"}. Empty for types that need no connection.
virtual QStringList connectionKinds() const { return {}; }

// Creates the view for one placement. The context outlives the view.
// Null shows the type's placeholder, as in M2 and M3.
virtual QWidget *createView(ItemContext &context, QWidget *parent) const { return nullptr; }
```

`Rendering` lives in `ItemType.h`. Web views are the ones the [memory policy](../web-views.md#memory-policy) unloads, so the core needs to know which types are heavy.

**`mirante/ItemContext.h`**, new:

```cpp
namespace mirante {

// The details of a connection, without its token. See docs/data-model.md#connection.
struct ConnectionDetails {
    QString id, kind, name;
    QUrl baseUrl;
    QJsonObject fields;
};

// What an item's code can reach. Made by the core for each placement.
class ItemContext {
public:
    virtual ~ItemContext() = default;

    virtual QString itemName() const = 0;
    virtual QJsonObject config() const = 0;

    // moduleSettings[<module id>][key] of the item's project, or undefined.
    virtual QJsonValue projectSetting(const QString &key) const = 0;

    // The first connection found for any of kinds, by the resolution order:
    // the item's connectionId, the project's override, the group default.
    virtual std::optional<ConnectionDetails> connection(const QStringList &kinds) const = 0;

    // The local path of a project folder on this device, or empty.
    virtual QString folderPath(const QString &folderId) const = 0;

    // A live web page of url, in the profile the item's group and module use for that
    // host, with the toolbar in the cell header. See docs/web-views.md.
    // The normal way for a web type to show a page.
    virtual QWidget *createWebView(const QUrl &url, QWidget *parent) = 0;

    // The profile the item's group and module use for url's host, for a module that
    // builds its own QWebEngineView. Declared, not defined, here; the module
    // links Qt WebEngine with QT_LIBS. See docs/decisions/0037-core-makes-web-views.md.
    virtual QWebEngineProfile *profile(const QUrl &url) = 0;

    // A grey message in the middle of the cell, such as "No GitLab connection
    // in this group", in the same style as the core's placeholders.
    virtual QWidget *createMessageView(const QString &text, QWidget *parent) = 0;

    // Changes the item's configuration through the Model.
    virtual void updateConfig(const QJsonObject &config) = 0;
};

}
```

- The [item context](../modules.md#item-context) table lists more members: `deviceSetting`, `status`, `gitHead` and `launchTerminal`. They arrive with the milestones that need them.
- `config()` reads the model each time, so it is current after an external change.
- `mirante_add_module()` gains a `QT_LIBS` argument, such as `QT_LIBS WebEngineWidgets`, for a module that uses `profile()`. The module-includes check allows Qt headers as before.
- A view made from `profile()` puts nothing in the cell header, and its module handles popups, permissions and downloads. The memory policy unloads it like any web view.

## Part 2: profiles

**`web/ProfileManager.h`:**

```cpp
class ProfileManager : public QObject {
public:
    // root is the folder that holds every profile's data. main.cpp passes
    // QStandardPaths::AppLocalDataLocation + "/profiles"; tests a temporary folder.
    explicit ProfileManager(const QString &root, QObject *parent = nullptr);
    ~ProfileManager() override;

    QWebEngineProfile *groupProfile(const QString &groupId, const QString &moduleId);  // "g-<groupId>-<moduleId>"
    QWebEngineProfile *sharedProfile(const QString &moduleId);                        // "shared-<moduleId>"
    QWebEngineProfile *appProfile();                                                  // "app"; used from M9

    // The module's profile in the group, or the module's shared one if
    // profileOverrides sends the URL's host there. Hosts match exactly,
    // ignoring case.
    QWebEngineProfile *profileFor(const Group &group, const QString &moduleId, const QUrl &url);

    // Clears the cookies and cache of every module's profile of the group,
    // then removes their folders once no page uses them. Called when the
    // user deletes a group.
    void removeGroup(const QString &groupId);

    QString storagePath(const QString &storageName) const;
};
```

- Profiles are made when first needed, with `ForcePersistentCookies` and the persistent permissions policy, as [web views](../web-views.md#profiles) asks.
- Every profile gets a user agent without the `QtWebEngine/<version>` part, so sites see an ordinary Chromium. Some identity providers refuse a browser that names itself embedded. This is the first thing to adjust if a login fails.
- Downloads: `downloadRequested` asks where to save, starting in the system download folder.
- A profile is deleted only when the user deletes a group in this app, not when a sync removes the group file. A group file that disappears for a moment would otherwise take its logins with it. See [decision 0039](../decisions/0039-profiles-deleted-only-by-local-delete.md).
- At shutdown `main.cpp` destroys the `MainWindow`, and with it every page, before the `ProfileManager`.

- The module of an item is the prefix of its type id, before the first dot. The placement's `ItemContext` passes it, so a module never names its own profile.
- `removeGroup` finds the group's profiles by the `g-<groupId>-` prefix of the folders, so it also removes those of modules missing from this build.

**Tests:** `tst_profile_manager`: the storage names and folders; a cookie set in group A's store is not in group B's; a cookie set for one module is not in another module's profile of the same group; an overridden host goes to the module's shared profile, another host does not; `removeGroup` removes the folders of every module.

## Part 3: the web panel

**`web/WebPanel.h`:**

```cpp
class WebPanel : public QWidget {
public:
    WebPanel(QWebEngineProfile *profile, const QUrl &url, QWidget *parent = nullptr);
    ~WebPanel() override;

    // Back, reload, the current address and "Open in browser", for the cell header.
    // The panel owns it; the cell header shows it.
    QWidget *toolbar() const;

    QWebEngineView *view() const;
};

// How a request for a new window is handled. Pure, for tests.
enum class NewWindow { LoginPopup, ExternalBrowser };
NewWindow classifyNewWindow(QWebEnginePage::WebWindowType type, bool userInitiated);
```

- **Popups.** A `WebDialog` window, which `window.open` with features makes, opens in a dialog in the same profile, and closes when the page closes it. OAuth sign-in uses this. The dialog is resizable, takes the size the page asks for but at least 500 × 600 pixels, and fits on the screen. Any other new window or tab opens in the external browser.
- **Permissions.** Clipboard read and write, and notifications, are asked once per host and profile with a short question in a bar above the page. Qt keeps the answer in the profile. Camera, microphone, screen capture and location are denied without asking. Showing notifications through the tray arrives in M8.
- **The toolbar.** Back is disabled when there is no history. The address is elided in the middle and selectable. "Open in browser" uses `QDesktopServices::openUrl`.
- **Load failure.** A failed load shows the error and a Reload button over the page.

**Tests:** `tst_web_panel`: `classifyNewWindow` for every window type; a panel loads a `data:` URL and its toolbar shows the address; back is disabled at the start.

## Part 4: connections and the group dialog

**`core/Connections.h`:**

```cpp
namespace mirante::connections {
// The connection for an item, by the resolution order in docs/data-model.md#connection.
// Only connections of the item's group, of one of kinds, count.
const Connection *resolve(const Model &model, const QString &taskId, const Item &item, const QStringList &kinds);
}
```

**The group dialog** gains two sections:

- **Connections:** a list with add, edit and remove. Each connection has a kind, a name and a base URL. The kinds offered are the `connectionKinds()` of every registered item type, from a new `ItemTypeRegistry::connectionKinds()`. One connection of each kind is the group default. A connection of a kind no type in this build uses is shown read-only, and kept.
- **Sites in the shared profile:** a list of host names, such as `acme.atlassian.net`, stored as `profileOverrides` with the value `"shared"`. The text explains that these sites share one login across all groups.

**Tests:** `tst_connections`: each step of the order, the wrong kind, a connection of another group. `tst_dialogs` gains a round trip of connections and shared sites.

## Part 5: placements and the memory policy

**`items/ViewPool.h`:**

```cpp
class ViewPool : public QObject {
public:
    explicit ViewPool(QObject *parent = nullptr);
    void setLimit(int hiddenViews);            // 8 by default
    // A placement with a heavy view. unload is called when the pool unloads it.
    void add(QObject *placement, std::function<void()> unload);
    void remove(QObject *placement);
    void setVisible(QObject *placement, bool visible);
    int hiddenCount() const;
};
```

When the number of hidden loaded placements is above the limit, the pool unloads those hidden longest. A placement that is unloaded and becomes visible loads again; the pool forgets it while it is unloaded.

**`items/Placement.h`:**

```cpp
// The body of an item's cell: the item's view, made when first visible.
class Placement : public QWidget {
public:
    Placement(const PlacementServices &services, const QString &taskId, const QString &itemId,
              QWidget *parent = nullptr);
    ~Placement() override;
    QWidget *view() const;                     // null before the first show, and while unloaded
signals:
    void headerWidgetChanged(QWidget *widget); // a web view's toolbar, or null
};

struct PlacementServices {
    Model *model = nullptr;
    const ItemTypeRegistry *types = nullptr;
    ProfileManager *profiles = nullptr;
    ViewPool *pool = nullptr;
    DeviceSettings *settings = nullptr;
};
```

- The placement holds an `ItemContext` of its own, implemented in `items/` on the model, the profile manager and the device settings.
- On the first `showEvent` it calls `createView`. If the type returns null, it shows the M2 placeholder, as now.
- A type whose `rendering()` is `Web` is added to the pool. Show and hide events report visibility. When unloaded, the view is deleted and a "Reload" placeholder takes its place, until the placement is shown again.
- When the item's type or `config` changes in the model, the view is made again. A change of name only changes the header.
- `createWebView` makes a `WebPanel` and emits `headerWidgetChanged` with its toolbar. `CellFrame` shows that widget in its header, between the name and the menu button.
- `CellFrame` gains `setBody(QWidget *)`, and `DashboardCellFactory` puts a `Placement` there when `LayoutServices::placements` is set. Tests that leave it null keep the placeholder.
- Two cells of one item have two placements and two views, as [decision 0003](../decisions/0003-view-per-placement.md) asks. Nothing more is needed for that.
- `DeviceSettings` gains `hiddenViewLimit()` with a default of 8, and its setter. A settings dialog is not part of M4; the value is read at start.

**Tests:** `tst_view_pool`: the limit, the order of unloading, a shown placement is never unloaded, a removed placement is forgotten. `tst_placement`, with a test item type: the view is made on first show and not before; a hidden web placement over the limit is unloaded and loads again when shown; a config change makes the view again.

## Part 6: a bigger view of an item

Signing in is easier on a large page than in a small cell. Two ways already exist, but only from the keyboard or the sidebar: `Ctrl+Shift+M` maximises the focused cell, and a double click on an item in the sidebar opens it alone in a tab. M4 makes both reachable from the cell itself.

- **Maximise button.** Every item cell header gets a button after the menu button, which maximises the cell, or restores it when it is maximised. Its tooltip names the shortcut. Maximising moves the same cell, so a web page keeps its state, including a login in progress.
- **Cell menu.** The item cell menu gains "Maximise" (or "Restore") and "Open Alone". "Open Alone" opens the item in the preview tab, as the sidebar's double click does. The tab has a view of its own, in the same profile, so a login made there reaches the cell when its page reloads.
- Both work on locked dashboards and in task dashboards shown inside root dashboards, because neither changes a layout.
- `LayoutServices` gains two callbacks, which `MainWindow` fills from `Shortcuts` and `openItem`:

```cpp
std::function<void(QWidget *cell)> toggleMaximised;
std::function<bool(const QWidget *cell)> isMaximised;
std::function<void(const QString &taskId, const QString &itemId)> openItemAlone;
```

- `Shortcuts` gains `toggleMaximised(QWidget *cell)` and `isMaximised(const QWidget *cell)`, so the button and the menu maximise a given cell, not only the focused one. Maximising a cell also focuses it.
- [ui](../ui.md) describes the button and the menu entries.

**Tests:** `tst_layout_editor` gains the two menu entries on locked and unlocked dashboards. `tst_shortcuts` gains maximising a cell that is not focused.

## Part 7: the item types

Every module declares `rendering()` and `connectionKinds()` for all of its types. The five web types get `createView`:

| Type | URL | Without what it needs |
|---|---|---|
| `web.page` | `url` | "Set the address in the item's settings." |
| `claude.session.web` | `url` | the same |
| `gitlab.mr.page` | `<baseUrl>/<project>/-/merge_requests/<iid>` | "No GitLab connection in this group." or "Set the project and number in the item's settings." |
| `gitlab.issue.page` | `<baseUrl>/<project>/-/issues/<iid>` | the same |
| `jira.issue.page` | `<baseUrl>/browse/<key>` | "No Jira connection in this group." or "Set the issue key in the item's settings." |

- An empty `project` uses the project's `gitlab` setting `project`, as [item types](../item-types.md#gitlab) says.
- Each module builds its URL in a pure function, so it is tested without a view.
- The GitLab types use the kind `gitlab`. The Jira types use `jira.cloud`, then `jira.dc`.

**Tests:** each of `gitlab`, `jira` and `claude` adds `tst_urls`, with a fake `ItemContext`, covering the URL and every message above. `web` adds the same for `web.page`.

## Part 8: detached windows

**`ui/DetachedWindow.h`:**

```cpp
// A top-level window holding one root dashboard. See docs/decisions/0024-detach-root-dashboard-tabs.md.
class DetachedWindow : public QWidget {
public:
    // Takes the page; it is moved, not made again, so its web pages keep their state.
    DetachedWindow(DashboardPage *page, DeviceSettings &settings, QWidget *parent = nullptr);
    DashboardPage *takePage();                 // gives the page back
    QString dashboardId() const;
signals:
    void putBackRequested(const QString &dashboardId);   // the window was closed
};
```

- The root tab menu gains "Detach into Window". The tab leaves the tab bar and its page moves into the new window.
- The window has the dashboard's name as its title, and a small toolbar with the dashboard's lock and "Put Back".
- Closing the window, or "Put Back", returns the page to the tab bar, at the end, and makes it current.
- Moving a page moves its cells. `LayoutView` makes again the cells whose type is not `reparentSafe`, through a new `LayoutView::remakeUnsafeCells()`.
- Opening a detached root dashboard from the sidebar, the switcher or the dashboard list raises its window.
- `DeviceSettings` gains `detachedGeometry(dashboardId)` and its setter, which store `saveGeometry()`. `Tabs` gains `detached`, the ids of the detached root dashboards. At start the app opens them again in their windows.
- Removing a root dashboard closes its window.

**Tests:** `tst_main_window` gains: detaching moves the page and keeps its widgets; closing puts it back; the detached list is saved and restored.

## Part 9: wiring

- `main.cpp` creates `ProfileManager` after the `Store` and before the `MainWindow`, and a `ViewPool` with the limit from `DeviceSettings`. It sets `Qt::AA_ShareOpenGLContexts` before the `QApplication`, as Qt WebEngine asks.
- `MainWindow` fills `LayoutServices::placements`.
- `EntityActions::remove` of a group calls `ProfileManager::removeGroup` after the model removes the group.
- The About dialog shows the Qt WebEngine version, which helps when a login fails.

## Tasks for parallel work

The rules, the roles and the brief are those of the [M2 plan](m2-app-skeleton.md#tasks-for-parallel-work), with `m2-app-skeleton.md` replaced by this plan. At most seven agents run at the same time.

The headers written by T0 are the interfaces. Where they differ in detail from the sketches in the parts above, the headers win.

| Task | Branch | Work | Owns | Needs |
|---|---|---|---|---|
| T0 | `m4-interfaces` | Qt WebEngine in `setup-qt`, CMake and CI, proven on all three platforms. The `src/web/` library. Every header named in this plan, with stubs that compile. The `ItemType` additions and `ItemContext.h`. `DashboardPage` moved out of `MainWindow.cpp`. `QT_LIBS` in `mirante_add_module()`. Updates [modules](../modules.md#item-context) for `createWebView` and `profile(url)`. | everything it creates; after merging, the headers belong to the orchestrator | none |
| T1 | `m4-profiles` | Part 2: `ProfileManager`. `tst_profile_manager`. Updates [web views](../web-views.md#profiles). | `src/web/ProfileManager.cpp`, its test | T0 |
| T2 | `m4-web-panel` | Part 3: `WebPanel`, its page class, the popup dialog and the permission bar. `tst_web_panel`. | `src/web/WebPanel.cpp` and new files in `src/web/` other than `ProfileManager.cpp`, its test | T0 |
| T3 | `m4-view-pool` | `ViewPool`, and the `DeviceSettings` additions of parts 5 and 8. `tst_view_pool`, `tst_device_settings` additions. | `src/items/ViewPool.cpp`, `src/core/DeviceSettings.cpp`, their tests | T0 |
| T4 | `m4-connections` | Part 4: `connections::resolve`, `ItemTypeRegistry::connectionKinds()` and the group dialog sections. `tst_connections`, `tst_dialogs` additions. Updates [data model](../data-model.md#connection) and [ui](../ui.md). | `src/core/Connections.cpp`, `src/items/ItemTypeRegistry.cpp`, `src/ui/GroupDialog.cpp`, their tests | T0 |
| T5 | `m4-module-views` | Part 7, for the seven modules. Each module's `tst_urls`. Updates each module's `README.md` and [item types](../item-types.md). | `src/modules/` | T0 |
| T6 | `m4-bigger-view` | Part 6 except the header button: the two cell menu entries and the `Shortcuts` methods. `tst_layout_editor` and `tst_shortcuts` additions. Updates [ui](../ui.md). | `src/layout/LayoutEditor.cpp`, `src/ui/Shortcuts.cpp`, their tests | T0 |
| T7 | `m4-placements` | Part 5 except the pool: `Placement`, its `ItemContext`, `CellFrame::setBody`, the header widget and the maximise button of Part 6, `DashboardCellFactory`. `tst_placement`. | `src/items/Placement*.cpp`, `src/items/ItemContextImpl.*`, `src/layout/CellFrame.cpp`, `src/layout/DashboardCellFactory.cpp`, `src/layout/LayoutView.cpp`, their tests | T1, T2, T3, T4 |
| T8 | `m4-detach` | Part 8: `DetachedWindow`, the tab menu entry, put back, geometry and restore. `tst_main_window` additions. Updates [ui](../ui.md#detached-windows). | `src/ui/DetachedWindow.cpp`, `src/ui/RootTabBar.cpp`, `src/ui/MainWindow.cpp`, `tests/ui/tst_main_window.cpp` | T3, T7 |
| T9 | `m4-wiring` | Part 9, and filling the `LayoutServices` callbacks of Part 6. | `src/main.cpp`, `src/ui/EntityActions.cpp`, `src/ui/MainWindow.cpp` (services only) | T6, T7, T8 |
| T10 | none | Done by the orchestrator: write `docs/plans/m4-checks.md`, run both presets, record the login results in [web views](../web-views.md#known-risk-logins-in-embedded-browsers), and tick the roadmap once the user has checked. | any | all |

T7 owns `LayoutView.cpp` for `remakeUnsafeCells()`, so T8 does not touch the layout library.

### Waves

| Wave | Tasks | At the same time |
|---|---|---|
| 0 | T0, then the user reviews the headers | 1 |
| 1 | T1, T2, T3, T4, T5, T6 | 6 |
| 2 | T7 | 1 |
| 3 | T8 | 1 |
| 4 | T9, then the login check | 1 |
| 5 | T10 | 1 |

The [roadmap](../roadmap.md#m4-web-views) asks for the login risk to be tested early. The first check by hand after T9 is logging in to claude.ai, gitlab.com and Jira Cloud. If a login fails, the fix belongs in `WebPanel` and `ProfileManager` (the user agent and popup handling), before T10.

## Checking

- **Automatic:** the tests above, by CTest locally and in CI on three platforms, with all modules and with none. Tests that load a page use `data:` URLs, never the network.
- **By hand on Windows:** the M4 acceptance criteria, listed in `docs/plans/m4-checks.md` by T10. They include: logins in three services surviving a restart, a login made in a maximised cell, a login in group A not visible in group B, a shared site seen by both groups, the same item live in two cells, unloading with a limit set low through the settings file, and detaching and putting back.

## Out of scope

- Tokens, kind fields, connection tests and project connection overrides in dialogs: M5.
- A settings dialog for the hidden-view limit: later; M4 reads the value from the settings file.
- Desktop notifications from web pages: M8.
- Keyboard shortcuts inside focused web pages: M8.
- The `dash://` scheme, the `app` profile's panels and `DashBridge`: M9. `appProfile()` exists from M4 but nothing uses it yet.
- Deleting profile folders of groups removed by another device.

## Ideas for later

- **Sign-in hosts always open in the login dialog.** Some sites open their sign-in with a plain `window.open` and no window features, which Chromium reports as a new tab. Part 3 sends such a window to the external browser, so the sign-in cookies land there and the login in the profile fails. A list of known sign-in hosts, such as `accounts.google.com`, `login.microsoftonline.com`, `id.atlassian.com` and `github.com/login`, could always open in the login dialog, whatever kind of window the page asks for. Worth adding if a login check in T10 shows the problem.
