# M5 code plan: servers and status

This plan implements [milestone M5](../roadmap.md#m5-servers-and-status). After M5, a token typed into GitLab's or Jira's settings screen is kept in the keychain. The app polls the GitLab and Jira servers of each profile, caches what they return, and computes attention. Badges appear in cell headers, the sidebar and Focus. Missing tokens, refused tokens and network errors are shown where they matter.

Native summary and list views stay placeholders until M6. The light actions are written into the clients in M5, but nothing in the user interface calls them until M6.

## Decisions

M5 depends on three new decisions:

- [Decision 0060](../decisions/0060-jira-cloud-or-data-center-by-setting.md): Jira Cloud or Data Center is a **Deployment** setting on the Jira screen, guessed from the site by default, with an **Email** field for Jira Cloud.
- [Decision 0061](../decisions/0061-attention-from-status-of-any-rendering.md): page types report status too, and the attention reasons that are events clear once the item has been seen.
- [Decision 0063](../decisions/0063-tokens-follow-a-new-workspace-id.md): when another device changed the workspace id, tokens move to the new id with the logins, as [decision 0062](../decisions/0062-logins-follow-a-new-workspace-id.md) does for logins.

Two smaller choices are made here and written into [integrations](../integrations.md#errors) by T2:

- **Which errors pause a server.** A `401` on any request, or a `403` on a `GET`, marks the profile's server as failing. A `403` on a `POST` is a missing permission for that action, such as a `read_api` token approving a merge request, and fails only that action.
- **The cache is in memory.** Status data is not written to disk. After a start, cells show no status until the first poll returns. A disk cache can come later if the wait is a problem.

## Changes to the architecture

- **A new library, `src/api/`.** It holds `SecretStore`, `ServerClient`, `StatusCache` and `Attention`, as the [architecture](../architecture.md#api) already lists. It links `core`, `Qt6::Network` and `qt6keychain`. `items` links `api`. `platform` no longer needs to link qtkeychain only to prove it builds.
- **The module interface gains `Qt6::Network`.** `ApiClient.h` uses `QNetworkRequest` and `QUrlQuery`. Modules still do not link Qt Network themselves; they reach the network only through `ApiClient`.
- **Modules describe their server; the core talks to it.** A module registers a `ServerType`: which screen holds the server, which field is the token, how to read the base URL, and how to add the token to a request. The core never names GitLab or Jira. Each module still builds its own paths and reads its own JSON.
- **Item types fetch status through a job.** A type that reports status implements `fetchStatus(StatusJob &)`. The `StatusCache` decides when an item is due and makes the job; the type makes its requests through the job's `ApiClient` and finishes with an `ItemStatus`.
- **Placements carry status to the cell.** A `Placement` subscribes to its item's status and tells the `StatusCache` when it is seen. `CellFrame` gets a status area and a refresh button.
- **Keychain keys follow decision 0059.** The key is `<workspace id>/<profile id>/<module id>/<field key>`. The roadmap, [data model](../data-model.md#servers-and-tokens), [ui](../ui.md#modules) and [modules](../modules.md#settings-screens) still show the older `<profile id>/…` form. T0 corrects them.

[architecture.md](../architecture.md) gets the new classes in the commit of the task that adds each one.

## Part 1: the module interface

### `ApiClient.h`, new

```cpp
namespace mirante {

// How a module's server is set up in a profile. A module registers one with
// ModuleRegistrar::setServerType(). See docs/modules.md#servers-and-tokens.
class ServerType
{
public:
    virtual ~ServerType() = default;

    // The settings screen that holds the server, such as "general", and the
    // key of its Secret field, such as "token".
    virtual QString screenId() const = 0;
    virtual QString tokenKey() const = 0;

    // The base URL in a profile's screen values, or an invalid URL if none
    // is set.
    virtual QUrl baseUrl(const QJsonObject &values) const = 0;

    // Why the values cannot be used even with a token, such as Jira Cloud
    // without an email, or empty if they can. Shown to the user.
    virtual QString problem(const QJsonObject &values) const { return {}; }

    // Adds the authentication to a request, such as a PRIVATE-TOKEN header.
    virtual void authorize(QNetworkRequest &request, const QString &token,
                           const QJsonObject &values) const = 0;
};

struct ApiReply
{
    enum class Error {
        None,
        NoServer,       // no base URL in the profile, or ServerType::problem()
        NoToken,        // no token on this device
        Refused,        // 401, or 403 on a GET: the server is now failing
        Forbidden,      // 403 on a POST
        NotFound,       // 404
        Http,           // any other status of 400 or more
        Network,        // no answer: DNS, TLS, timeout, offline
        Paused,         // the server is failing; nothing was sent
    };
    Error error = Error::None;
    int httpStatus = 0;
    QJsonDocument json;         // the body, parsed; from the cache after a 304
    QByteArray body;
    QList<QPair<QByteArray, QByteArray>> headers;
    QString message;            // for the user, such as "Not found (404)"
};

struct GetOptions
{
    // Follow the Link header's "next" page and join the arrays, up to
    // maxRows rows. GitLab lists use it. Jira pages itself.
    bool followLinks = false;
    int maxRows = 100;
};

// A module's server in one profile. Every request is asynchronous. The
// callback is dropped if receiver is destroyed first. See docs/integrations.md.
class ApiClient
{
public:
    virtual ~ApiClient() = default;

    // The base URL, or invalid. Paths below are relative to it, so a server
    // installed at a path works: "api/v4/user", not "/api/v4/user".
    virtual QUrl baseUrl() const = 0;

    // GETs send If-None-Match with the last ETag of the same URL and profile,
    // and a 304 returns the cached body.
    virtual void get(const QString &path, const QUrlQuery &query, const GetOptions &options,
                     QObject *receiver, std::function<void(const ApiReply &)> done) = 0;
    virtual void post(const QString &path, const QJsonDocument &body,
                      QObject *receiver, std::function<void(const ApiReply &)> done) = 0;

    // Adds the authentication to a request made by the module itself. False
    // if the token is not loaded yet or not set. Prefer get() and post().
    virtual bool authorize(QNetworkRequest &request) = 0;

    // The raw token, loaded from the keychain if needed. Use it only where a
    // request is not enough, such as passing the token to a command; each
    // call needs a comment saying why authorize() does not work there.
    virtual void secret(QObject *receiver, std::function<void(std::optional<QString>)> done) = 0;
};

} // namespace mirante
```

### `Status.h`, new

```cpp
namespace mirante {

// A live reason to act now. See decision 0061.
struct AttentionReason
{
    QString key;                // stable while the reason is the same, such as "status:10001"
    QString text;               // such as "Pipeline failed"
    bool notify = false;        // also a desktop notification, from M8
    bool clearsOnView = false;  // an event: stops counting once the item was seen
};

struct ItemStatus
{
    enum class State { None, Ok, NoServer, NoToken, Refused, NotFound, Error };
    State state = State::None;
    QString summary;            // one line, such as "Open · pipeline failed"
    QString message;            // why, for every state but Ok
    QJsonObject data;           // what the type's views need, read by M6 summaries
    QList<AttentionReason> attention;
    std::optional<bool> closed; // the work is finished, such as a merged MR
    QDateTime updatedAt;        // when data last came from the server
};

// One fetch of one item's status. The StatusCache makes it.
class StatusJob
{
public:
    virtual ~StatusJob() = default;
    virtual QJsonObject config() const = 0;
    virtual QJsonValue projectSetting(const QString &key) const = 0;
    virtual QJsonObject moduleSettings(const QString &screenId) const = 0;
    virtual ApiClient &server() = 0;
    // A QObject that lives as long as the job, for the receiver of requests.
    virtual QObject *owner() = 0;
    // Ends the job. Call exactly once; the job is deleted afterwards.
    virtual void finish(const ItemStatus &status) = 0;
};

// An item's entry in the status cache, for its views.
class ItemStatusSource
{
public:
    virtual ~ItemStatusSource() = default;
    virtual ItemStatus current() const = 0;
    virtual void refresh() = 0;
    // Called with each new status until receiver is destroyed.
    virtual void subscribe(QObject *receiver, std::function<void(const ItemStatus &)> changed) = 0;
};

} // namespace mirante
```

`ItemStatus::State` mirrors `ApiReply::Error`. A module may turn any reply error into the matching state with `statusFromReply(reply)`, an inline helper in `Status.h`.

### Additions to existing headers

- **`ItemType`:** `virtual bool hasStatus() const { return false; }` and `virtual void fetchStatus(StatusJob &job) const`, which by default finishes with `State::None`.
- **`ItemContext`:** `virtual ApiClient *server() = 0`, the module's server in the item's profile, or null for a module without a `ServerType`; and `virtual ItemStatusSource &status() = 0`.
- **`ModuleRegistrar`:** `setServerType(std::unique_ptr<ServerType>)`, and `addProjectSetting(const Field &field)`. Project settings use every field type except `Secret`, `Folder` and `Image`.
- **`Fields.h`:** the comment on `Secret` names the new key form.

### A fake for module tests

A header-only target, `mirante::module_api_testing`, holds `mirante/testing/FakeApiClient.h` and `FakeStatusJob.h`. The fake client answers each path from a map of recorded responses, which the module's tests load from `tests/data/*.json`, and records the requests made. `mirante_add_module()` links it to the module's tests only.

**Tests:** none in T0 beyond the build; each module's tests use the fakes.

## Part 2: secrets

`SecretStore` in `src/api/`:

```cpp
class SecretStore : public QObject
{
public:
    explicit SecretStore(std::unique_ptr<SecretBackend> backend, QObject *parent = nullptr);

    // Loads the token into memory if needed. Values are kept in memory after
    // the first read, so later reads answer at once.
    void read(const QString &key, QObject *receiver, std::function<void(std::optional<QString>)> done);
    // The token if already loaded, without asking the keychain.
    std::optional<QString> cached(const QString &key) const;
    // Known: loaded and set, loaded and not set, or not loaded yet.
    enum class State { Unknown, Set, NotSet };
    State state(const QString &key) const;

    void write(const QString &key, const QString &value);
    void remove(const QString &key);
    // Removes the given keys.
    void removeAll(const QStringList &keys);
    // Moves each of the keys from one workspace id to another: reads the
    // token under the old key, writes it under the new one unless one is
    // there already, then removes the old one. done is called once, after
    // all keys. Keys being moved answer read() only after their move.
    void moveAll(const QString &fromWorkspaceId, const QString &toWorkspaceId,
                 const QStringList &keysWithoutWorkspace, std::function<void()> done);

signals:
    void changed(const QString &key);
    void failed(const QString &key, const QString &message);
};

QString secretKey(const QString &workspaceId, const QString &profileId,
                  const QString &moduleId, const QString &fieldKey);
```

- **Backends.** `SecretBackend` has asynchronous `read`, `write` and `remove`. `KeychainBackend` uses qtkeychain's `ReadPasswordJob`, `WritePasswordJob` and `DeletePasswordJob`, with the service `mirante` and `setAutoDelete(true)`. `MemoryBackend` keeps values in a map, for tests and for `--no-keychain`.
- **Listing.** No keychain lists its keys portably. `removeAll` and `moveAll` therefore take the keys: the core builds them from the profiles and every module's `Secret` fields, with `tokenKeys(model, registry)`, a free function in `SecretStore.h` that gives `<profile id>/<module id>/<field key>` for every profile and every `Secret` field of the build.
- **A changed workspace id.** `moveAll` implements [decision 0063](../decisions/0063-tokens-follow-a-new-workspace-id.md).
- **Errors.** A keychain error, such as a locked keychain or no Secret Service on Linux, emits `failed`. The main window's warning bar shows "Could not use the keychain: <message>". The token then counts as not set.
- **`secretKey`** moves from `EntityActions` to `SecretStore.h`. `EntityActions::secretKey` calls it until T9 removes it.

**Tests:** `tst_secret_store` with `MemoryBackend`: a read loads once; `cached` and `state` follow reads, writes and removes; `changed` is emitted; `removeAll` removes the listed keys only; `moveAll` moves set tokens, keeps a token already under the new key and removes the old one, skips keys not set, holds back a `read()` of a key until its move is done, and calls `done` once. `KeychainBackend` is checked by hand, because CI runners have no unlocked keychain.

## Part 3: the server client

`ServerClient` in `src/api/` implements `ApiClient` for one workspace, profile and module. `ServerClients` owns them, one per such triple, made on first use, and the one `QNetworkAccessManager`.

- **Settings.** Each request reads the profile's screen values through the module's `ModuleSettings`, and the base URL and problem through the `ServerType`. No base URL, or a problem, gives `NoServer` without sending anything.
- **Token.** Before the first request the client reads the token from the `SecretStore`. Requests wait in the queue while it loads. No token gives `NoToken`.
- **Queue.** At most four requests run at once for each client. The rest wait in order.
- **ETags.** For each full URL the client keeps the last `ETag` and body. A `GET` sends `If-None-Match`; a `304` answers with the kept body. The memory is bounded: the 500 most recent URLs per client.
- **Paging.** With `followLinks`, the client follows `Link: <…>; rel="next"` and joins the JSON arrays until `maxRows`.
- **Rate limits.** On `429` the client waits for `Retry-After` seconds, or 30 if absent, and sends again, at most three times. Other requests of that client wait too.
- **Failing.** A `401`, or a `403` on a `GET`, sets the client failing and emits `ServerClients::failingChanged(profileId, moduleId)`. While failing, requests return `Paused` without being sent. The client clears failing when its token changes (`SecretStore::changed`) or its screen values change (`Model::profilesChanged`).
- **Timeouts.** 30 seconds per request, with `QNetworkRequest::setTransferTimeout`.
- **Headers.** `Accept: application/json`, and `User-Agent: Mirante/<version>`.

**Tests:** `tst_server_client` with a `QNetworkAccessManager` subclass that answers from a table instead of the network: the base URL with a path joins correctly; no base URL and no token give their errors without a request; four at a time; ETag sent and `304` served from memory; paging joins arrays and stops at `maxRows`; `429` waits and retries; `401` pauses the client and a token change resumes it; `403` on `POST` does not pause it; a destroyed receiver gets no callback.

## Part 4: the status cache and attention

`StatusCache` in `src/api/`:

```cpp
class StatusCache : public QObject
{
public:
    StatusCache(Model &model, const ItemTypeRegistry &types, ServerClients &clients,
                DeviceSettings &settings, QObject *parent = nullptr);

    ItemStatus status(const QString &itemId) const;
    ItemStatusSource &source(const QString &itemId);
    void refresh(const QString &itemId);

    // What is due. See docs/integrations.md#polling-and-caching.
    void setVisible(const QString &itemId, bool visible);     // counted per placement
    void noteTaskOpened(const QString &taskId);
    // The user looked at the item: its clearsOnView reasons become seen.
    void markSeen(const QString &itemId);

signals:
    void statusChanged(const QString &itemId);
};
```

- **Due items.** On each tick, the items of types with `hasStatus()` that are in a visible cell, in a task opened in the last 30 minutes, or in an active task of Urgent or High priority. Items in the trash, and items whose module is missing, are never due. An item is fetched when it is due and its status is older than the interval, or has none.
- **Interval.** `DeviceSettings::pollInterval()`, two minutes by default, between 30 seconds and one hour. The timer ticks every 15 seconds and fetches what is due, so a new visible item does not wait for a whole interval.
- **At once.** An item that becomes visible with no status, a refresh, and a change to the item's configuration or profile fetch at once.
- **One job per item.** A fetch for an item already being fetched is skipped. A job that does not finish in 90 seconds is dropped with `State::Error`.
- **Errors keep data.** A new status with an error keeps the previous `data`, `summary`, `attention` and `updatedAt`, and sets `state` and `message`. Cells then show the age of the data.
- **Removal.** When an item is removed, its entry and its seen keys go.

`Attention` in `src/api/` turns statuses into counts:

- An item **needs attention** when its status has a reason that counts: one without `clearsOnView`, or one whose key is not among the item's seen keys.
- **The first status** of an item without a seen record records its `clearsOnView` keys as seen. See decision 0061.
- `markSeen` adds the current `clearsOnView` keys to the item's record. Keys no longer reported are dropped from it, so the record stays small.
- `taskCount(taskId)` is the number of the task's items that need attention. `projectCount` and `groupCount` sum the active tasks below. Tasks in the trash and done tasks count zero.
- `reasons(itemId)` gives the reasons that count, for tooltips.
- It emits `countsChanged()`, compressed to once per event loop pass.

`DeviceSettings` gains `pollInterval()`, `setPollInterval()`, and `seenAttention(workspaceId, itemId)` with its setter and a remover.

**Tests:** `tst_status_cache` with a fake item type and `FakeApiClient`: what is due for each of the three rules; the 30 minutes after a task is opened; trash and missing modules are never due; refresh fetches at once; one job per item; an error keeps the data; a stuck job times out. `tst_attention`: the first look records keys; a new key counts until seen; states never clear; sums for tasks, projects and groups leave out done and trashed tasks. `tst_device_settings` additions.

## Part 5: project settings

- `ItemTypeRegistry` keeps each module's project setting fields from `addProjectSetting`, in order, and gives them with `projectSettings(moduleId)`.
- The project dialog gets a section for each module with project settings, titled with the module's name, after the folders. Each section is a `FieldForm` of that module's fields. Values are read from and written to `project.moduleSettings.<module id>`. Keys the form does not show are kept, and so are the settings of modules not in the build. An empty value removes its key; an empty module object is removed.
- OK is refused while a section reports a problem, as in the item dialog.
- The [project dialog](../ui.md) section of ui.md describes the sections.

**Tests:** `tst_registry` additions: fields are kept per module and in order, and `Secret` fields are refused. `tst_dialogs` additions: the sections appear for modules with project settings; values round-trip; unknown keys and missing modules' settings are kept; empty values remove their keys.

## Part 6: the gitlab module

- **Settings.** The `GeneralScreen` is unchanged. `GitLabServerType` reads `baseUrl` with the existing `baseUrlOf()`, and adds `PRIVATE-TOKEN`.
- **Project setting.** `project`, a text field, "GitLab project path". The existing types already use it when their own `project` is empty.
- **`GitLabClient`.** A thin class over `ApiClient`, with one method per endpoint in [integrations](../integrations.md#gitlab): merge request, approvals, the three merge request lists, the latest pipeline for a branch, jobs, retry and cancel of a pipeline, retry of a job, approve, the two comment endpoints, issue, issue list, and current user. Each takes the parsed arguments, such as a project path and an iid, encodes the project path, and returns the `ApiReply`. Lists use `followLinks`.
- **Current user.** `GET api/v4/user` is asked once per status job; the client's ETag memory makes it cheap. Its `id` and `username` are what "me" means below.
- **Status:**

| Type | Requests | Attention | Closed |
|---|---|---|---|
| MR (summary, page) | merge request; approvals; current user | "Pipeline failed" (`pipeline-failed`, notify) when `head_pipeline.status` is `failed`; "Review requested" (`review`) when the user is a reviewer and has not approved; "Approved" (`approved`, clears on view) when the user is the author and `approved` is true; "Has conflicts" (`conflicts`) when `has_conflicts` is true and the user is the author | `state` is `merged` or `closed` |
| Issue (summary, page) | issue | none | `state` is `closed` |
| CI Pipeline | for `mergeRequest`, the merge request, then its source branch; for `fixed`, the branch; then the latest pipeline and its jobs | "Pipeline failed" (`pipeline-failed:<pipeline id>`, notify) | never |

  The summary reads like "Open · 2 of 3 approvals · pipeline failed". The CI Pipeline with `mainFolder` reports `State::None` with "Follows the main folder from M7"; [M7](../roadmap.md#m7-quick-add-folders-and-notes) adds it.
- **Lists** report no status in M5.

**Tests:** `tst_client` (paths, encoding, paging options, POST bodies), `tst_status` on recorded responses (each row of the table, a merged MR, an MR with no pipeline, `NoToken` and `NotFound` passed through), `tst_settings` additions for the server type. Recorded responses live in `src/modules/gitlab/tests/data/`, trimmed by hand from real answers, with names and emails replaced.

## Part 7: the jira module

- **Settings.** The `GeneralScreen` gains **Deployment** and **Email**, after Base URL, as [decision 0060](../decisions/0060-jira-cloud-or-data-center-by-setting.md) describes. `deploymentOf(values)` returns Cloud or Data Center.
- **`JiraServerType`.** Basic authentication with `email:token` on Cloud, `Authorization: Bearer <token>` on Data Center. `problem()` asks for the email on Cloud without one.
- **Project setting.** `projectKey`, a text field, "Jira project key".
- **`JiraClient`.** One method per endpoint in [integrations](../integrations.md#jira), with the API root chosen by the deployment: issue with `expand=renderedFields`, search (`search/jql` with `nextPageToken` on Cloud, `search` with `startAt` on Data Center, up to 100 rows), transitions read and do, comment, and current user. A comment body is ADF on Cloud and plain text on Data Center; `JiraClient::comment` takes plain text and builds the body.
- **Status:**

| Type | Requests | Attention | Closed |
|---|---|---|---|
| Issue (summary, page) | issue, with `fields=summary,status,assignee,priority,issuetype,updated` | "Status changed to <name>" (`status:<status id>`, clears on view) | `status.statusCategory.key` is `done` |

  The summary reads like "In Review · assigned to Ana".
- **Lists** report no status in M5.

**Tests:** `tst_client` for both deployments (API root, authentication header, search paging styles, ADF and plain comments), `tst_status` on recorded responses from both, `tst_settings` additions: the guess from the host, the explicit choices, the missing email.

## Part 8: showing status

**Cell header.** `CellFrame` gains:

```cpp
struct CellStatus
{
    enum class Kind { None, Attention, Problem, Stale };
    Kind kind = Kind::None;
    int count = 0;          // attention reasons that count
    QString text;           // short, such as "Token not set"
    QString tooltip;        // the reasons, or the message and the data's age
    bool canFix = false;    // shows a button that emits fixRequested
};
void setStatus(const CellStatus &status);
void setRefreshVisible(bool visible);
signals: void refreshRequested(); void fixRequested();
```

- **Attention** is a small round badge in the attention colour, with the count, before the menu button. Its tooltip lists the reasons.
- **Problem** is a warning pill: "Token not set", "Token refused", "No server set" or "Not found". For the first three the pill is a button that opens the profile's settings screen of the module. Its tooltip gives the message and the profile's name.
- **Stale** is grey text such as "12 min ago", shown when the last fetch failed with a network or HTTP error but older data exists.
- **Refresh** is a tool button before the menu button, shown for items of types with `hasStatus()`. It fetches at once.

**Placement.** For a type with `hasStatus()`, a placement subscribes to its item's status and emits `statusChanged(CellStatus)`, built from the `ItemStatus` and `Attention::reasons`. It calls `StatusCache::setVisible` from the same visibility it reports to the `ViewPool`. After two seconds visible in the active window, it calls `markSeen`. `refresh()` and `openSettings()` are public slots. `ItemContextImpl` implements `server()` and `status()`. `PlacementServices` gains `StatusCache *status`, `ServerClients *servers` and `std::function<void(QString profileId, QString moduleId)> openModuleSettings`.

**Sidebar.** `SidebarModel` gains `AttentionRole`, the count from `Attention` for tasks, projects and groups. `SidebarDelegate` paints the count in a small round badge at the right of the row, in the attention colour. The model listens to `countsChanged` and emits `dataChanged` for the rows whose count changed.

**Focus.** Each row shows the task's badge. The "Needs attention first" toggle, a check box at the right of the group combo box, moves tasks with attention to the top, keeping the usual order within each part. Its state is a device setting.

**Main window.** The warning bar gets one line per failing server: "GitLab in the profile Work refused the token." with a "Set Token…" button that opens the screen. The line goes when the client stops failing.

**Settings window.** A Status section with "Check servers every" and a spin box in minutes, from 1 to 60. It changes `DeviceSettings::pollInterval()`.

**Settings screens.** A `Secret` field shows its state in its placeholder: "Set on this device. Type to replace it." or "Not set on this device.", read from `SecretStore::state`, loading it when unknown. A **Remove** button beside a set token removes it on OK. Typing nothing keeps the token.

**Profiles.** Deleting a profile removes its tokens on this device, for every module's `Secret` fields. Clear Data… leaves tokens alone, as its warning already says module settings stay.

**Tests:** `tst_cell_frame` (new): each kind shows the right widgets and the fix button emits. `tst_placement` additions: the status reaches the frame, visibility reaches the cache, `markSeen` after two seconds visible. `tst_sidebar_model` additions: the role and the sums. `tst_focus_view` additions: the toggle's order. `tst_field_form` additions: the secret placeholder and Remove. `tst_entity_actions` additions: a typed token is written with the right key; deleting a profile removes its tokens. `tst_main_window` additions: a failing server shows its line, and the line goes when it recovers.

## Part 9: wiring

- `main.cpp` creates `SecretStore` with the `KeychainBackend`, or the `MemoryBackend` with `--no-keychain`, then `ServerClients`, then `StatusCache`, after the `Store` and before the `ProfileManager`, as [ownership](../architecture.md#ownership-and-lifetime) lists. They are destroyed in reverse order.
- `EntityActions::setSaveSecret` writes to the `SecretStore`. The log line of M4 goes.
- `MainWindow` fills the new `PlacementServices` members, calls `noteTaskOpened` when a task dashboard opens in a tab or slot, gives `Attention` to the sidebar and Focus, and fills the warning bar from `ServerClients::failingChanged` and `SecretStore::failed`.
- Opening another workspace drops the status cache's entries and the clients, because the workspace id is part of every key.
- Where `MainWindow` moves logins with `ProfileManager::adoptProfilesOf()` after the id changed on another device ([decision 0062](../decisions/0062-logins-follow-a-new-workspace-id.md)), it also calls `SecretStore::moveAll` with the remembered id, the new one and `tokenKeys()`. It is not called when this device gave the id.
- Tests and the `tools/run.ps1` checks with a scratch settings file pass `--no-keychain`, so they never touch the user's keychain. The checks by hand of the keychain itself run without it.

## Tasks for parallel work

The rules, the roles and the brief are those of the [M2 plan](m2-app-skeleton.md#tasks-for-parallel-work), with `m2-app-skeleton.md` replaced by this plan. At most three agents run at the same time, as the user asked for M5.

The headers written by T0 are the interfaces. Where they differ in detail from the sketches in the parts above, the headers win. The main differences:

- `ItemContext::server()` and `status()` return pointers and have defaults that return null, so the module tests' fake contexts need no change.
- `markSeen` is on `Attention`, not `StatusCache`, since the seen keys are attention's.
- `src/api/` reaches the modules through `ModuleDirectory`, which `ItemTypeRegistry` implements, so `api` does not depend on `items`.
- `ServerClients` is the public class in `ServerClient.h`; each `ApiClient` it makes is private to `ServerClient.cpp`.
- `CellStatus` is in `items/CellStatus.h`, so placements can build it. `Placement` emits `cellStatusChanged`, and has the slots `refresh()` and `openSettings()`. `PlacementServices` names the callback `openServerSettings`.
- `StatusCache` has `poll()`, `setPolling()` and `setClock()` for tests and the timer, and `clear()` for another workspace.
- `DeviceSettings` has `hasSeenAttention()`, to tell an item never recorded from one with no seen keys, and `focusAttentionFirst()`.
- The api services reach the window together, as `ApiServices`, through `MainWindow::setApiServices()`. `EntityActions` gains `setSecretStore()`. `FieldForm` gains `setSecretState()` and `removedSecrets()`.
- `ItemTypeRegistry` stores the server type already; T4 fills in `addProjectSetting()`.

| Task | Branch | Work | Owns | Needs |
|---|---|---|---|---|
| T0 | `m5-interfaces` | The `src/api/` library and its CMake, with `Qt6::Network` and qtkeychain. `Qt6::Network` on the module interface. `ApiClient.h`, `Status.h` and the additions to `ItemType`, `ItemContext`, `Module.h` and `Fields.h`. The `module_api_testing` target and its two fakes, linked by `mirante_add_module()`. Every new header of parts 2 to 8 with stubs that compile, including `CellStatus`, the `PlacementServices` members, `SidebarModel::AttentionRole` and the `DeviceSettings` declarations. The keychain key form corrected in the roadmap, data model, ui and modules documents, and [modules](../modules.md#servers-and-tokens) updated for `ServerType`, `fetchStatus` and project settings. | everything it creates; after merging, the headers belong to the orchestrator | none |
| T1 | `m5-secrets` | Part 2: `SecretStore`, its two backends, `tokenKeys()` and `moveAll`. `tst_secret_store`. Updates [sync](../sync.md#secrets), and [web views](../web-views.md#profiles) where it says tokens do not move. | `src/api/SecretStore*`, `src/api/*Backend*`, their tests | T0 |
| T2 | `m5-server-client` | Part 3: `ServerClient` and `ServerClients`. `tst_server_client`. Updates [integrations](../integrations.md#errors) with the two choices at the top of this plan. | `src/api/ServerClient*`, its test | T0 |
| T3 | `m5-status-cache` | Part 4: `StatusCache`, `Attention` and the `DeviceSettings` additions. `tst_status_cache`, `tst_attention`, `tst_device_settings` additions. Updates [integrations](../integrations.md#polling-and-caching) and [sync](../sync.md#device-settings). | `src/api/StatusCache*`, `src/api/Attention*`, `src/core/DeviceSettings.cpp`, their tests | T0 |
| T4 | `m5-project-settings` | Part 5: project settings in the registry and the project dialog. `tst_registry` and `tst_dialogs` additions. Updates [ui](../ui.md) and [data model](../data-model.md#project). | `src/items/ItemTypeRegistry.cpp`, `src/ui/ProjectDialog.cpp`, their tests | T0 |
| T5 | `m5-gitlab` | Part 6. Updates the module's `README.md`, [item types](../item-types.md#gitlab) and [integrations](../integrations.md#gitlab). | `src/modules/gitlab/` | T0 |
| T6 | `m5-jira` | Part 7. Updates the module's `README.md`, [item types](../item-types.md), [integrations](../integrations.md#jira) and [data model](../data-model.md#servers-and-tokens). | `src/modules/jira/` | T0 |
| T7 | `m5-badges` | Part 8, the widgets only: `CellFrame`'s status area and refresh button, the sidebar badge, Focus's badge and toggle, the Status section of the settings window. Widgets are fed by hand in tests. Updates [ui](../ui.md#attention). | `src/layout/CellFrame.cpp`, `src/ui/SidebarModel.cpp`, `src/ui/SidebarDelegate.cpp`, `src/ui/FocusView.cpp`, `src/ui/SettingsDialog.cpp`, their tests | T0 |
| T8 | `m5-placement-status` | Part 8, the placement: subscribing, visibility, `markSeen`, `ItemContextImpl::server()` and `status()`, `DashboardCellFactory` connecting a placement to its frame. `tst_placement` additions. | `src/items/Placement.cpp`, `src/items/ItemContextImpl.cpp`, `src/layout/DashboardCellFactory.cpp`, their tests | T2, T3, T7 |
| T9 | `m5-wiring` | Part 9, and the rest of Part 8: the secret state in `FieldForm`, `EntityActions` writing and removing tokens, the warning bar lines, `noteTaskOpened`, moving tokens with the logins. `tst_field_form`, `tst_entity_actions` and `tst_main_window` additions. | `src/main.cpp`, `src/ui/EntityActions.cpp`, `src/ui/FieldForm.cpp`, `src/ui/MainWindow.cpp`, their tests | T1, T4, T5, T6, T8 |
| T10 | none | Done by the orchestrator: write `docs/plans/m5-checks.md`, run the `dev`, `no-modules`, `release` and `ci` presets, and tick the roadmap once the user has checked. | any | all |

T5 and T6 need the module interface only, so they run beside the core tasks. T7 owns `SettingsDialog.cpp` because the Status section is one more widget section there; T3 owns only the `DeviceSettings` side.

### Waves

| Wave | Tasks | At the same time |
|---|---|---|
| 0 | T0, then the user reviews the headers | 1 |
| 1 | T1, T2, T3 | 3 |
| 2 | T5, T6, T7 | 3 |
| 3 | T4, T8 | 2 |
| 4 | T9 | 1 |
| 5 | T10 | 1 |

The core services come first, because T8 and T9 need them. The two modules and the widgets come next. T4 needs nothing but T0, so it fills the third wave beside T8.

The first check by hand after T9 is a real token against gitlab.com and the user's Jira Cloud site. If either is refused, the fix belongs in that module's `ServerType` before T10.

## Checking

- **Automatic:** the tests above, by CTest locally and in CI on three platforms, with all modules and with none. No test uses the network or the real keychain.
- **By hand on Windows:** the M5 acceptance criteria, listed in `docs/plans/m5-checks.md` by T10. They include:
  - a GitLab token and a Jira Cloud token typed into their screens, kept after a restart, and visible in Windows Credential Manager under `mirante` with the expected key;
  - removing a token, and deleting a profile, removes the entry;
  - after changing the `id` in `workspace.mirante` by hand while the app is closed, as another device would, the tokens are under the new id when the workspace opens, and the GitLab item still shows its status;
  - a GitLab MR (page) item of a merge request with a failed pipeline shows a badge in its cell, its task, project and group, and Focus;
  - a Jira Issue (page) item: changing the issue's status in Jira shows "Status changed", which clears after the cell has been visible for two seconds;
  - the refresh button fetches at once; the poll interval in the settings window takes effect;
  - "Needs attention first" in Focus;
  - a wrong token shows "Token refused" in the cell and a line in the warning bar, and stops polling; fixing it resumes;
  - no token shows "Token not set" with a button that opens the right screen of the right profile;
  - unplugging the network keeps the badges and shows the age;
  - a Jira Data Center site, if one is at hand; otherwise noted as not checked.

## Out of scope

- Native summary and list views, and the buttons of the light actions: M6. The clients' action methods are in M5, tested, but not called.
- Polling while hidden in the tray, the tray count and desktop notifications: M8. `notify` is reported but nothing reads it.
- The CI Pipeline following the main folder's branch: M7.
- A disk cache of status data.
- A "Test" button on the settings screens.
- Jira development information (linked branches and merge requests).
- Custom certificate authorities.
- Status for built-in panels: M9.

## Ideas for later

- **A Test button on the settings screens.** It would ask for the current user with the typed token before OK, and say whether the token works and whose it is. It needs the dialog to send a request through a token that is not stored yet.
- **Status in tooltips of sidebar items.** The Items row of a task could show each item's summary line, such as "Open · pipeline failed", without opening the task.
- **Revisit the GitLab and Jira project settings.** GitLab's project path is only a fallback for items with no project of their own, and Jira's project key is not used yet. The user kept both for now. M6 decides whether they earn their place, for example as the default project of quick add and of list presets, or are removed.
