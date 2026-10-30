// Tests Placement and its ItemContext with test item types: when the view is
// made, unloading by the ViewPool, making the view again after a change, the
// header widget, what the context reads from the model, the page of showPage(),
// adding an item to the task, the item's status in the cell header, view keys,
// folder mappings, and the context's device settings and files. No test uses
// the network or the keychain.
#include "api/Attention.h"
#include "api/SecretBackend.h"
#include "api/SecretStore.h"
#include "api/ServerClient.h"
#include "api/StatusCache.h"
#include "core/DeviceSettings.h"
#include "core/Model.h"
#include "core/Profiles.h"
#include "core/Store.h"
#include "items/ItemContextImpl.h"
#include "items/ItemTypeRegistry.h"
#include "items/Placement.h"
#include "items/ViewPool.h"
#include "web/ProfileManager.h"
#include "web/WebPanel.h"

#include <mirante/ApiClient.h>
#include <mirante/ItemContext.h>
#include <mirante/ItemType.h>
#include <mirante/Module.h>
#include <mirante/Status.h>

#include <QAbstractButton>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QWebEngineView>

#include <map>
#include <memory>

using namespace mirante;

namespace {

// How many views each test type made. Reset before each test.
int viewsMade = 0;
// The configurations that onConfigChanged() reported to test.keys views.
// Reset before each test.
QList<QJsonObject> configsReported;
// The context of the view made last, for a test to act as the view. Reset
// before each test.
ItemContext *lastContext = nullptr;

// A view that shows the item's "text" setting, so a test can tell an old view
// from a new one.
class FakeView : public QLabel
{
public:
    FakeView(const QString &text, QWidget *parent)
        : QLabel(text, parent)
    {
        setObjectName(QStringLiteral("fakeView"));
    }
};

// test.native: a cheap view. test.web: a fake heavy view. test.none: no view.
// test.page: a real web page of the item's "url". test.keys: a cheap view
// that writes the key "note" itself, as a Notes view would.
class TestType : public ItemType
{
public:
    explicit TestType(const QString &id)
        : m_id(id)
    {
    }

    QString id() const override { return m_id; }
    QString name() const override { return QStringLiteral("Type ") + m_id; }
    QIcon icon() const override { return {}; }

    Rendering rendering() const override
    {
        return m_id == QStringLiteral("test.native") || m_id == QStringLiteral("test.none")
                       || m_id == QStringLiteral("test.keys")
                   ? Rendering::Native
                   : Rendering::Web;
    }

    QStringList viewKeys() const override
    {
        return m_id == QStringLiteral("test.keys") ? QStringList{QStringLiteral("note")} : QStringList();
    }

    QWidget *createView(ItemContext &context, QWidget *parent) const override
    {
        if (m_id == QStringLiteral("test.none"))
            return nullptr;
        ++viewsMade;
        lastContext = &context;
        if (m_id == QStringLiteral("test.page"))
            return context.createWebView(QUrl(context.config().value(QStringLiteral("url")).toString()), parent);
        auto *view = new FakeView(context.config().value(QStringLiteral("text")).toString(), parent);
        if (m_id == QStringLiteral("test.keys"))
            context.onConfigChanged(view, [](const QJsonObject &config) { configsReported << config; });
        return view;
    }

private:
    QString m_id;
};

void registerTest(ModuleRegistrar &registrar)
{
    for (const char *id : {"test.native", "test.web", "test.none", "test.page", "test.keys"})
        registrar.addItemType(std::make_unique<TestType>(QString::fromLatin1(id)));
    Field editor;
    editor.key = QStringLiteral("editor");
    editor.label = QStringLiteral("Editor");
    editor.defaultValue = QStringLiteral("code");
    registrar.addDeviceSetting(editor);
    Field lines;
    lines.key = QStringLiteral("lines");
    lines.label = QStringLiteral("Lines");
    lines.type = Field::Type::Integer;
    registrar.addDeviceSetting(lines);
}

// How stat.thing ends each fetch: with the status given for the item, found
// by the "id" in its configuration, or with what one GET through its server
// answers. Reset before each test.
std::map<QString, ItemStatus> nextStatus;
bool statusFromServer = false;
int statusFetches = 0;

// stat.thing: a type that reports status, of the module "stat", which has a
// server.
class StatusType : public ItemType
{
public:
    QString id() const override { return QStringLiteral("stat.thing"); }
    QString name() const override { return QStringLiteral("Thing"); }
    QIcon icon() const override { return {}; }
    bool hasStatus() const override { return true; }

    void fetchStatus(StatusJob &job) const override
    {
        ++statusFetches;
        if (statusFromServer) {
            job.server().get(QStringLiteral("api/thing"), {}, {}, job.owner(),
                             [&job](const ApiReply &reply) { job.finish(statusFromReply(reply)); });
            return;
        }
        job.finish(nextStatus[job.config().value(QStringLiteral("id")).toString()]);
    }
};

class StatServerType : public ServerType
{
public:
    QString screenId() const override { return QStringLiteral("server"); }
    QString tokenKey() const override { return QStringLiteral("token"); }
    QUrl baseUrl(const QJsonObject &values) const override
    {
        return QUrl(values.value(QStringLiteral("baseUrl")).toString());
    }
    void authorize(QNetworkRequest &request, const QString &token, const QJsonObject &) const override
    {
        request.setRawHeader("PRIVATE-TOKEN", token.toUtf8());
    }
};

void registerStat(ModuleRegistrar &registrar)
{
    registrar.addItemType(std::make_unique<StatusType>());
    registrar.setServerType(std::make_unique<StatServerType>());
}

ItemStatus okStatus(QList<AttentionReason> attention = {})
{
    ItemStatus status;
    status.state = ItemStatus::State::Ok;
    status.attention = std::move(attention);
    return status;
}

ItemStatus failedStatus(ItemStatus::State state, const QString &message)
{
    ItemStatus status;
    status.state = state;
    status.message = message;
    return status;
}

AttentionReason reason(const QString &key, const QString &text, bool clearsOnView = false)
{
    AttentionReason r;
    r.key = key;
    r.text = text;
    r.clearsOnView = clearsOnView;
    return r;
}

// A reply with the HTTP status the network's handler gives.
class FakeReply : public QNetworkReply
{
public:
    FakeReply(QNetworkAccessManager::Operation op, const QNetworkRequest &request, QObject *parent)
        : QNetworkReply(parent)
    {
        setOperation(op);
        setRequest(request);
        setUrl(request.url());
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
    }

    void respond(int status)
    {
        if (isFinished())
            return;
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, status);
        if (status >= 400)
            setError(QNetworkReply::ProtocolInvalidOperationError, QStringLiteral("HTTP %1").arg(status));
        setFinished(true);
        emit finished();
    }

    void abort() override
    {
        if (isFinished())
            return;
        setError(QNetworkReply::OperationCanceledError, QStringLiteral("Aborted"));
        setFinished(true);
        emit finished();
    }

    bool isSequential() const override { return true; }

protected:
    qint64 readData(char *, qint64) override { return 0; }
};

// Answers every request with one HTTP status in the next event loop pass,
// and records the requests. Nothing goes to the network.
class FakeNetwork : public QNetworkAccessManager
{
public:
    int status = 200;
    QList<QNetworkRequest> sent;

protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &request, QIODevice *) override
    {
        auto *reply = new FakeReply(op, request, this);
        sent.append(request);
        const int answer = status;
        QTimer::singleShot(0, reply, [reply, answer] { reply->respond(answer); });
        return reply;
    }
};

Item makeItem(const QString &id, const QString &type, const QJsonObject &config)
{
    Item item;
    item.id = id;
    item.type = type;
    item.name = QStringLiteral("Item ") + id;
    item.config = config;
    return item;
}

// Group g1, which uses Default, with a server in Default's settings, project
// p1 with a setting of the test module, and task t1 with one item of each
// type.
void fill(Model &model)
{
    Group group;
    group.id = QStringLiteral("g1");
    group.name = QStringLiteral("Group");
    Profile standard;
    standard.id = kDefaultProfile;
    standard.moduleSettings = QJsonObject{
        {QStringLiteral("test"),
         QJsonObject{{QStringLiteral("server"),
                      QJsonObject{{QStringLiteral("baseUrl"), QStringLiteral("https://gitlab.example.com")}}}}},
        {QStringLiteral("stat"),
         QJsonObject{{QStringLiteral("server"),
                      QJsonObject{{QStringLiteral("baseUrl"), QStringLiteral("https://stat.example/")}}}}}};

    Project project;
    project.id = QStringLiteral("p1");
    project.groupId = QStringLiteral("g1");
    project.name = QStringLiteral("Project");
    project.moduleSettings = QJsonObject{
        {QStringLiteral("test"), QJsonObject{{QStringLiteral("project"), QStringLiteral("acme/app")}}}};

    Task task;
    task.id = QStringLiteral("t1");
    task.projectId = QStringLiteral("p1");
    task.title = QStringLiteral("Task");
    task.items.push_back(makeItem(QStringLiteral("n1"), QStringLiteral("test.native"), {{QStringLiteral("text"), QStringLiteral("one")}}));
    task.items.push_back(makeItem(QStringLiteral("w1"), QStringLiteral("test.web"), {{QStringLiteral("text"), QStringLiteral("w1")}}));
    task.items.push_back(makeItem(QStringLiteral("w2"), QStringLiteral("test.web"), {{QStringLiteral("text"), QStringLiteral("w2")}}));
    task.items.push_back(makeItem(QStringLiteral("x1"), QStringLiteral("test.none"), {}));
    task.items.push_back(makeItem(QStringLiteral("p1"), QStringLiteral("test.page"),
                                  {{QStringLiteral("url"), QStringLiteral("data:text/plain,first")}}));
    task.items.push_back(makeItem(QStringLiteral("s1"), QStringLiteral("stat.thing"), {{QStringLiteral("id"), QStringLiteral("s1")}}));
    task.items.push_back(makeItem(QStringLiteral("k1"), QStringLiteral("test.keys"),
                                  {{QStringLiteral("text"), QStringLiteral("keys")}, {QStringLiteral("note"), QStringLiteral("a")}}));

    model.reset({group}, {project}, {task}, {});
    model.setProfiles({standard});
}

// Changes one item of task t1 through the model.
void changeItem(Model &model, const QString &itemId, const std::function<void(Item &)> &change)
{
    Task task = *model.task(QStringLiteral("t1"));
    for (Item &item : task.items) {
        if (item.id == itemId)
            change(item);
    }
    model.setTask(task);
}

QString viewText(const Placement &placement)
{
    const auto *label = qobject_cast<const QLabel *>(placement.view());
    return label ? label->text() : QString();
}

QWidget *pageOf(const Placement &placement)
{
    return placement.findChild<QWidget *>(QStringLiteral("itemPage"));
}

const QUrl pageUrl(QStringLiteral("data:text/plain,page"));

// Clicks "Back to summary" in the placement's header widget.
bool clickBack(const Placement &placement)
{
    QWidget *header = placement.headerWidget();
    auto *back = header ? header->findChild<QAbstractButton *>(QStringLiteral("backToSummary")) : nullptr;
    if (!back || back->text() != QStringLiteral("Back to summary"))
        return false;
    back->click();
    return true;
}

bool showsText(const QWidget *widget, const QString &text)
{
    for (const QLabel *label : widget->findChildren<QLabel *>()) {
        if (label->isVisibleTo(widget) && label->text().contains(text))
            return true;
    }
    return false;
}

} // namespace

class TestPlacement : public QObject
{
    Q_OBJECT

private:
    ItemTypeRegistry types;
    std::unique_ptr<Model> model;
    std::unique_ptr<ViewPool> pool;
    PlacementServices services;

    // The status services, made by enableStatus().
    std::unique_ptr<QTemporaryDir> statusDir;
    std::unique_ptr<QSettings> statusSettings;
    std::unique_ptr<DeviceSettings> statusDevice;
    std::unique_ptr<SecretStore> secrets;
    std::unique_ptr<FakeNetwork> network;
    std::unique_ptr<ServerClients> servers;
    std::unique_ptr<StatusCache> cache;
    std::unique_ptr<Attention> attention;
    // What openServerSettings was called with, as "profile/module".
    QStringList settingsOpened;

    std::unique_ptr<Placement> make(const QString &itemId)
    {
        auto placement = std::make_unique<Placement>(services, QStringLiteral("t1"), itemId);
        placement->resize(300, 200);
        return placement;
    }

    // Gives the placements a status cache, Attention and real server clients
    // over the fake network, in the workspace "ws".
    void enableStatus()
    {
        model->setWorkspaceId(QStringLiteral("ws"));
        statusDir = std::make_unique<QTemporaryDir>();
        QVERIFY(statusDir->isValid());
        statusSettings = std::make_unique<QSettings>(statusDir->filePath(QStringLiteral("settings.ini")),
                                                     QSettings::IniFormat);
        statusDevice = std::make_unique<DeviceSettings>(statusSettings.get());
        secrets = std::make_unique<SecretStore>(makeMemoryBackend());
        network = std::make_unique<FakeNetwork>();
        servers = std::make_unique<ServerClients>(*model, types, *secrets, network.get());
        cache = std::make_unique<StatusCache>(*model, types, *servers, *statusDevice);
        attention = std::make_unique<Attention>(*model, *cache, *statusDevice);
        services.settings = statusDevice.get();
        services.status = cache.get();
        services.attention = attention.get();
        services.servers = servers.get();
        services.openServerSettings = [this](const QString &profileId, const QString &moduleId) {
            settingsOpened << profileId + QLatin1Char('/') + moduleId;
        };
    }

    // Fetches s1 again, with this status.
    void refreshWith(const ItemStatus &status)
    {
        nextStatus[QStringLiteral("s1")] = status;
        cache->refresh(QStringLiteral("s1"));
    }

private slots:
    void initTestCase()
    {
        types.registerModule(QStringLiteral("test"), &registerTest);
        types.registerModule(QStringLiteral("stat"), &registerStat);
        types.setSettingsReader([this](const QString &profileId, const QString &moduleId, const QString &screenId) {
            return model ? model->moduleSettings(profileId, moduleId, screenId) : QJsonObject();
        });
    }

    void init()
    {
        viewsMade = 0;
        configsReported.clear();
        lastContext = nullptr;
        nextStatus.clear();
        statusFromServer = false;
        statusFetches = 0;
        settingsOpened.clear();
        model = std::make_unique<Model>();
        fill(*model);
        pool = std::make_unique<ViewPool>();
        services = PlacementServices{};
        services.model = model.get();
        services.types = &types;
        services.pool = pool.get();
    }

    void cleanup()
    {
        services = PlacementServices{};
        attention.reset();
        cache.reset();
        servers.reset();
        network.reset();
        secrets.reset();
        statusDevice.reset();
        statusSettings.reset();
        statusDir.reset();
        pool.reset();
        model.reset();
    }

    void viewIsMadeOnFirstShow()
    {
        auto placement = make(QStringLiteral("n1"));
        QVERIFY(!placement->view());
        QCOMPARE(viewsMade, 0);

        placement->show();
        QVERIFY(placement->view());
        QCOMPARE(viewText(*placement), QStringLiteral("one"));
        QCOMPARE(viewsMade, 1);

        // Hiding and showing again keeps the view.
        QPointer<QWidget> view = placement->view();
        placement->hide();
        placement->show();
        QCOMPARE(placement->view(), view.data());
        QCOMPARE(viewsMade, 1);

        // A native view is not given to the pool.
        placement->hide();
        QCOMPARE(pool->hiddenCount(), 0);
    }

    void typeWithoutViewShowsPlaceholder()
    {
        auto placement = make(QStringLiteral("x1"));
        placement->show();
        QVERIFY(!placement->view());
        QVERIFY(showsText(placement.get(), QStringLiteral("Type: Type test.none")));
        QVERIFY(showsText(placement.get(), QStringLiteral("Name: Item x1")));

        // A new name shows in the placeholder.
        changeItem(*model, QStringLiteral("x1"), [](Item &item) { item.name = QStringLiteral("Renamed"); });
        QVERIFY(showsText(placement.get(), QStringLiteral("Name: Renamed")));
    }

    // A type that is not registered, though its module is in the build, has
    // been removed from the module: the cell says so, not that the module is
    // missing. The item is kept unchanged. See decision 0084.
    void removedTypeShowsPlaceholder()
    {
        const QJsonObject config{{QStringLiteral("sessionId"), QStringLiteral("abc")}};
        Task task = *model->task(QStringLiteral("t1"));
        task.items.push_back(makeItem(QStringLiteral("r1"), QStringLiteral("test.removed"), config));
        task.items.push_back(makeItem(QStringLiteral("m1"), QStringLiteral("gone.thing"), config));
        model->setTask(task);

        auto removed = make(QStringLiteral("r1"));
        removed->show();
        QVERIFY(!removed->view());
        QVERIFY(showsText(removed.get(), QStringLiteral("This item type is no longer available: test.removed.")));
        QVERIFY(!showsText(removed.get(), QStringLiteral("module")));

        auto missing = make(QStringLiteral("m1"));
        missing->show();
        QVERIFY(!missing->view());
        QVERIFY(showsText(missing.get(),
                          QStringLiteral("This item needs the gone module, which this build does not include.")));

        const Item *item = model->item(QStringLiteral("t1"), QStringLiteral("r1"));
        QVERIFY(item);
        QCOMPARE(item->type, QStringLiteral("test.removed"));
        QCOMPARE(item->name, QStringLiteral("Item r1"));
        QCOMPARE(item->config, config);
        QCOMPARE(viewsMade, 0);
    }

    void hiddenWebViewIsUnloadedAndLoadsAgain()
    {
        pool->setLimit(1);
        auto first = make(QStringLiteral("w1"));
        auto second = make(QStringLiteral("w2"));
        first->show();
        second->show();
        QVERIFY(first->view() && second->view());
        QPointer<QWidget> firstView = first->view();

        // Two hidden views are over the limit of one, so the one hidden
        // longest is unloaded.
        first->hide();
        second->hide();
        QVERIFY(!first->view());
        QVERIFY(second->view());
        QCOMPARE(pool->hiddenCount(), 1);
        QVERIFY(first->findChild<QWidget *>(QStringLiteral("reloadPlaceholder")));
        QTRY_VERIFY(firstView.isNull());

        // Shown again, it loads again.
        first->show();
        QVERIFY(first->view());
        QCOMPARE(viewText(*first), QStringLiteral("w1"));
        QCOMPARE(viewsMade, 3);
        QVERIFY(!first->findChild<QWidget *>(QStringLiteral("reloadPlaceholder"))
                || !first->findChild<QWidget *>(QStringLiteral("reloadPlaceholder"))->isVisibleTo(first.get()));

        // It is back in the pool: hiding it unloads the other hidden one.
        first->hide();
        QVERIFY(!second->view());
        QVERIFY(first->view());
    }

    void reloadButtonLoadsAgain()
    {
        pool->setLimit(0);
        auto placement = make(QStringLiteral("w1"));
        placement->show();
        placement->hide();
        QVERIFY(!placement->view());

        auto *reload = placement->findChild<QPushButton *>();
        QVERIFY(reload);
        QCOMPARE(reload->text(), QStringLiteral("Reload"));
        pool->setLimit(8);
        reload->click();
        QVERIFY(placement->view());
        QCOMPARE(viewsMade, 2);
        QCOMPARE(pool->hiddenCount(), 1);
    }

    void destroyedPlacementLeavesThePool()
    {
        auto placement = make(QStringLiteral("w1"));
        placement->show();
        placement->hide();
        QCOMPARE(pool->hiddenCount(), 1);
        placement.reset();
        QCOMPARE(pool->hiddenCount(), 0);
    }

    void configChangeMakesTheViewAgain()
    {
        auto placement = make(QStringLiteral("n1"));
        placement->show();
        QPointer<QWidget> old = placement->view();

        changeItem(*model, QStringLiteral("n1"), [](Item &item) { item.config[QStringLiteral("text")] = QStringLiteral("two"); });
        QCOMPARE(viewsMade, 2);
        QVERIFY(placement->view() != old.data());
        QCOMPARE(viewText(*placement), QStringLiteral("two"));
        QTRY_VERIFY(old.isNull());

        // A change of name alone keeps the view.
        QPointer<QWidget> current = placement->view();
        changeItem(*model, QStringLiteral("n1"), [](Item &item) { item.name = QStringLiteral("New name"); });
        QCOMPARE(viewsMade, 2);
        QCOMPARE(placement->view(), current.data());

        // A change of type makes it again.
        changeItem(*model, QStringLiteral("n1"), [](Item &item) { item.type = QStringLiteral("test.web"); });
        QCOMPARE(viewsMade, 3);
    }

    void hiddenPlacementMakesItsNewViewWhenShown()
    {
        auto placement = make(QStringLiteral("n1"));
        placement->show();
        placement->hide();
        changeItem(*model, QStringLiteral("n1"), [](Item &item) { item.config[QStringLiteral("text")] = QStringLiteral("two"); });
        QVERIFY(!placement->view());
        QCOMPARE(viewsMade, 1);
        placement->show();
        QCOMPARE(viewText(*placement), QStringLiteral("two"));
        QCOMPARE(viewsMade, 2);
    }

    void viewCanChangeItsOwnConfig()
    {
        // updateConfig from the view's own code makes a new view. The old one
        // is deleted later, so its code may still run.
        auto placement = make(QStringLiteral("n1"));
        placement->show();
        ItemContextImpl context(services, QStringLiteral("t1"), QStringLiteral("n1"));
        context.updateConfig({{QStringLiteral("text"), QStringLiteral("three")}});
        QCOMPARE(model->item(QStringLiteral("t1"), QStringLiteral("n1"))->config.value(QStringLiteral("text")).toString(),
                 QStringLiteral("three"));
        QCOMPARE(viewText(*placement), QStringLiteral("three"));
    }

    void removedItemDropsTheView()
    {
        auto placement = make(QStringLiteral("w1"));
        placement->show();
        QPointer<QWidget> view = placement->view();
        placement->hide();
        QCOMPARE(pool->hiddenCount(), 1);

        model->removeTask(QStringLiteral("t1"));
        QVERIFY(!placement->view());
        QCOMPARE(pool->hiddenCount(), 0);
        QTRY_VERIFY(view.isNull());
        placement->show();
        QVERIFY(!placement->view());
        QVERIFY(showsText(placement.get(), QStringLiteral("Removed")));
    }

    void contextReadsTheModel()
    {
        QSettings settings(QDir(QDir::tempPath()).filePath(QStringLiteral("mirante-tst-placement.ini")), QSettings::IniFormat);
        settings.clear();
        DeviceSettings device(&settings);
        device.setFolderPath(QStringLiteral("f1"), QStringLiteral("C:/src/app"));
        services.settings = &device;

        ItemContextImpl context(services, QStringLiteral("t1"), QStringLiteral("n1"));
        QCOMPARE(context.itemName(), QStringLiteral("Item n1"));
        QCOMPARE(context.config().value(QStringLiteral("text")).toString(), QStringLiteral("one"));
        QCOMPARE(context.projectSetting(QStringLiteral("project")).toString(), QStringLiteral("acme/app"));
        QVERIFY(context.projectSetting(QStringLiteral("missing")).isUndefined());
        QCOMPARE(context.folderPath(QStringLiteral("f1")), QStringLiteral("C:/src/app"));
        QVERIFY(context.folderPath(QStringLiteral("f2")).isEmpty());

        QCOMPARE(context.moduleSettings(QStringLiteral("server")).value(QStringLiteral("baseUrl")).toString(),
                 QStringLiteral("https://gitlab.example.com"));

        // Module settings of the item's own module, in the item's profile.
        QVERIFY(context.moduleSettings(QStringLiteral("general")).isEmpty());
        const QJsonObject standardValues{{QStringLiteral("a"), 1}};
        const QJsonObject workValues{{QStringLiteral("a"), 2}};
        Profile work;
        work.id = QStringLiteral("work");
        work.name = QStringLiteral("Work");
        model->setProfile(work);
        model->setModuleSettings(kDefaultProfile, QStringLiteral("test"), QStringLiteral("general"), standardValues);
        model->setModuleSettings(QStringLiteral("work"), QStringLiteral("test"), QStringLiteral("general"), workValues);
        model->setModuleSettings(kDefaultProfile, QStringLiteral("other"), QStringLiteral("general"), workValues);
        QCOMPARE(context.moduleSettings(QStringLiteral("general")), standardValues);
        QVERIFY(context.moduleSettings(QStringLiteral("none")).isEmpty());
        changeItem(*model, QStringLiteral("n1"), [](Item &item) { item.profile = QStringLiteral("work"); });
        QCOMPARE(context.moduleSettings(QStringLiteral("general")), workValues);
        // The server is that of the item's profile, which has none.
        QVERIFY(context.moduleSettings(QStringLiteral("server")).isEmpty());
        changeItem(*model, QStringLiteral("n1"), [](Item &item) { item.profile.clear(); });

        // Read on each call, so a change shows at once.
        changeItem(*model, QStringLiteral("n1"), [](Item &item) {
            item.name = QStringLiteral("Changed");
            item.config[QStringLiteral("text")] = QStringLiteral("two");
        });
        QCOMPARE(context.itemName(), QStringLiteral("Changed"));
        QCOMPARE(context.config().value(QStringLiteral("text")).toString(), QStringLiteral("two"));

        // Without a profile manager there is no page, but no crash either.
        QVERIFY(!context.profile());
        std::unique_ptr<QWidget> web(context.createWebView(QUrl(QStringLiteral("data:text/plain,x")), nullptr));
        QVERIFY(qobject_cast<QLabel *>(web.get()));

        std::unique_ptr<QWidget> message(context.createMessageView(QStringLiteral("No connection"), nullptr));
        auto *label = qobject_cast<QLabel *>(message.get());
        QVERIFY(label);
        QCOMPARE(label->text(), QStringLiteral("No connection"));
        QVERIFY(label->alignment() == Qt::AlignCenter);
        services.settings = nullptr;
    }

    // A web view is made again when the item's profile changes: through the
    // item, its group, or the workspace's list. A native view is not.
    void profileChangeMakesTheViewAgain()
    {
        auto web = make(QStringLiteral("w1"));
        auto native = make(QStringLiteral("n1"));
        web->show();
        native->show();
        QPointer<QWidget> webView = web->view();
        QPointer<QWidget> nativeView = native->view();
        QVERIFY(webView && nativeView);

        const Profile standard = *model->profile(kDefaultProfile);
        Profile work;
        work.id = QStringLiteral("work");
        work.name = QStringLiteral("Work");
        Profile bank;
        bank.id = QStringLiteral("bank");
        bank.name = QStringLiteral("Bank");

        // A new profile that nobody uses changes nothing.
        model->setProfiles({standard, work, bank});
        QCOMPARE(web->view(), webView.data());

        // The group's default.
        Group group = *model->group(QStringLiteral("g1"));
        group.profile = QStringLiteral("work");
        model->setGroup(group);
        QVERIFY(web->view() && web->view() != webView.data());
        QCOMPARE(native->view(), nativeView.data());
        webView = web->view();

        // The item's own profile.
        changeItem(*model, QStringLiteral("w1"), [](Item &item) { item.profile = QStringLiteral("bank"); });
        QVERIFY(web->view() && web->view() != webView.data());
        webView = web->view();

        // The group's default no longer reaches it.
        group.profile.clear();
        model->setGroup(group);
        QCOMPARE(web->view(), webView.data());

        // Renaming its profile changes nothing.
        bank.name = QStringLiteral("Bank 2");
        model->setProfile(bank);
        QCOMPARE(web->view(), webView.data());

        // A change of its profile's module settings does: the
        // view may have read them. Another profile's change does not.
        Profile changedWork = work;
        changedWork.moduleSettings = QJsonObject{{QStringLiteral("test"), QJsonObject{}}};
        model->setProfile(changedWork);
        QCOMPARE(web->view(), webView.data());
        bank.moduleSettings = standard.moduleSettings;
        model->setProfile(bank);
        QVERIFY(web->view() && web->view() != webView.data());
        webView = web->view();

        // Deleting its profile falls back to the group's, Default.
        model->setProfiles({standard, work});
        QVERIFY(web->view() && web->view() != webView.data());
        QCOMPARE(native->view(), nativeView.data());
    }

    // Clearing a profile drops the views in it at once, so the profile can be
    // released, and makes them again when the clearing is done.
    void clearingAProfileDropsItsViewsUntilDone()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        auto profiles = std::make_unique<ProfileManager>(root.path());
        services.profiles = profiles.get();
        auto web = make(QStringLiteral("w1"));
        auto native = make(QStringLiteral("n1"));
        web->show();
        native->show();
        QPointer<QWidget> webView = web->view();
        QPointer<QWidget> nativeView = native->view();
        QVERIFY(webView && nativeView);

        QSignalSpy cleared(profiles.get(), &ProfileManager::profileCleared);
        profiles->clearProfile(kDefaultProfile);
        QVERIFY(!web->view());
        QCOMPARE(native->view(), nativeView.data());
        QTRY_COMPARE_WITH_TIMEOUT(cleared.count(), 1, 15000);
        QVERIFY(web->view() && web->view() != webView.data());
        QCOMPARE(native->view(), nativeView.data());

        web.reset();
        native.reset();
        services.profiles = nullptr;
        profiles.reset();
    }

    void moduleOfTheType()
    {
        QCOMPARE(moduleOfType(QStringLiteral("gitlab.mr.page")), QStringLiteral("gitlab"));
        QCOMPARE(moduleOfType(QStringLiteral("notes")), QStringLiteral("notes"));
    }

    // A real page of a data: URL, in a profile of a temporary folder.
    void webViewPutsItsToolbarInTheHeader()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        root.setAutoRemove(false);
        auto profiles = std::make_unique<ProfileManager>(root.path());
        services.profiles = profiles.get();
        {
            auto placement = make(QStringLiteral("p1"));
            QSignalSpy header(placement.get(), &Placement::headerWidgetChanged);
            placement->show();

            auto *panel = qobject_cast<WebPanel *>(placement->view());
            QVERIFY(panel);
            QCOMPARE(panel->view()->page()->profile(), profiles->profile(kDefaultProfile));
            QCOMPARE(header.count(), 1);
            QCOMPARE(header.at(0).at(0).value<QWidget *>(), panel->toolbar());
            QCOMPARE(placement->headerWidget(), panel->toolbar());

            // A new address makes a new page. The old toolbar leaves the
            // header before the new one comes.
            header.clear();
            changeItem(*model, QStringLiteral("p1"), [](Item &item) {
                item.config[QStringLiteral("url")] = QStringLiteral("data:text/plain,second");
            });
            auto *second = qobject_cast<WebPanel *>(placement->view());
            QVERIFY(second && second != panel);
            QCOMPARE(header.count(), 2);
            QVERIFY(!header.at(0).at(0).value<QWidget *>());
            QCOMPARE(header.at(1).at(0).value<QWidget *>(), second->toolbar());

            // Unloading removes it.
            header.clear();
            pool->setLimit(0);
            placement->hide();
            QVERIFY(!placement->view());
            QCOMPARE(header.count(), 1);
            QVERIFY(!header.at(0).at(0).value<QWidget *>());
            QVERIFY(!placement->headerWidget());
        }
        // The pages went with the placement, before their profiles.
        services.profiles = nullptr;
        profiles.reset();
        // Chromium closes some profile files a moment after the profile goes.
        const QString path = root.path();
        QVERIFY(QTest::qWaitFor([&] { return QDir(path).removeRecursively(); }, 15000));
    }

    // Without a profile manager the page is a message, with the same way back.
    void pageShowsInPlaceOfTheView()
    {
        auto placement = make(QStringLiteral("n1"));
        placement->show();
        QPointer<QWidget> view = placement->view();
        QSignalSpy header(placement.get(), &Placement::headerWidgetChanged);

        lastContext->showPage(pageUrl);
        QWidget *page = pageOf(*placement);
        QVERIFY(page);
        QVERIFY(page->isVisible());
        QCOMPARE(placement->view(), view.data());
        QVERIFY(!view->isVisibleTo(placement.get()));
        QCOMPARE(viewsMade, 1);
        QCOMPARE(header.count(), 1);
        QCOMPARE(header.at(0).at(0).value<QWidget *>(), placement->headerWidget());
        QVERIFY(placement->headerWidget()->findChild<QAbstractButton *>(QStringLiteral("backToSummary")));

        // It is in the pool, though the view is not.
        placement->hide();
        QCOMPARE(pool->hiddenCount(), 1);
    }

    void backReturnsTheSameView()
    {
        auto placement = make(QStringLiteral("n1"));
        placement->show();
        QPointer<QWidget> view = placement->view();
        lastContext->showPage(pageUrl);
        QPointer<QWidget> page = pageOf(*placement);
        QPointer<QWidget> pageHeader = placement->headerWidget();
        QSignalSpy header(placement.get(), &Placement::headerWidgetChanged);

        QVERIFY(clickBack(*placement));
        QCOMPARE(placement->view(), view.data());
        QVERIFY(view->isVisible());
        QCOMPARE(viewsMade, 1);
        QVERIFY(!placement->headerWidget());
        QCOMPARE(header.count(), 1);
        QVERIFY(!header.at(0).at(0).value<QWidget *>());
        QTRY_VERIFY(page.isNull() && pageHeader.isNull());

        // It left the pool with the page.
        placement->hide();
        QCOMPARE(pool->hiddenCount(), 0);
    }

    void unloadReturnsTheView()
    {
        pool->setLimit(0);
        auto placement = make(QStringLiteral("n1"));
        placement->show();
        QPointer<QWidget> view = placement->view();
        lastContext->showPage(pageUrl);
        QPointer<QWidget> page = pageOf(*placement);
        QVERIFY(placement->headerWidget());

        placement->hide();
        QCOMPARE(placement->view(), view.data());
        QVERIFY(!placement->headerWidget());
        QVERIFY(!placement->findChild<QWidget *>(QStringLiteral("reloadPlaceholder")));
        QCOMPARE(pool->hiddenCount(), 0);
        QTRY_VERIFY(page.isNull());

        placement->show();
        QVERIFY(view->isVisible());
        QCOMPARE(viewsMade, 1);
    }

    void configChangeDropsThePage()
    {
        auto placement = make(QStringLiteral("n1"));
        placement->show();
        QPointer<QWidget> old = placement->view();
        lastContext->showPage(pageUrl);
        QPointer<QWidget> page = pageOf(*placement);

        // A change of name alone keeps it.
        changeItem(*model, QStringLiteral("n1"), [](Item &item) { item.name = QStringLiteral("New name"); });
        QCOMPARE(pageOf(*placement), page.data());

        changeItem(*model, QStringLiteral("n1"), [](Item &item) { item.config[QStringLiteral("text")] = QStringLiteral("two"); });
        QVERIFY(!placement->headerWidget());
        QCOMPARE(viewText(*placement), QStringLiteral("two"));
        QVERIFY(placement->view() != old.data());
        QVERIFY(placement->view()->isVisible());
        QCOMPARE(viewsMade, 2);
        QTRY_VERIFY(page.isNull());
        placement->hide();
        QCOMPARE(pool->hiddenCount(), 0);
    }

    void furtherPageReplacesThePage()
    {
        auto placement = make(QStringLiteral("n1"));
        placement->show();
        QPointer<QWidget> view = placement->view();
        lastContext->showPage(pageUrl);
        QPointer<QWidget> first = pageOf(*placement);
        QPointer<QWidget> firstHeader = placement->headerWidget();

        lastContext->showPage(QUrl(QStringLiteral("data:text/plain,other")));
        QVERIFY(placement->headerWidget() && placement->headerWidget() != firstHeader.data());
        QTRY_VERIFY(first.isNull() && firstHeader.isNull());
        QVERIFY(pageOf(*placement));
        QVERIFY(!view->isVisibleTo(placement.get()));

        // One way back is enough.
        QVERIFY(clickBack(*placement));
        QVERIFY(view->isVisible());
        QVERIFY(!placement->headerWidget());
    }

    // The placement's one pool entry follows what is loaded: the page, then
    // the web view again.
    void pageOverAWebView()
    {
        auto placement = make(QStringLiteral("w1"));
        placement->show();
        QPointer<QWidget> view = placement->view();
        lastContext->showPage(pageUrl);
        placement->hide();
        QCOMPARE(pool->hiddenCount(), 1);

        // The page goes first, and the view after it if the limit still asks.
        pool->setLimit(0);
        QVERIFY(!placement->view());
        QVERIFY(placement->findChild<QWidget *>(QStringLiteral("reloadPlaceholder")));
        QCOMPARE(pool->hiddenCount(), 0);
        QTRY_VERIFY(view.isNull());

        // Back leaves the web view in the pool.
        pool->setLimit(8);
        placement->show();
        QCOMPARE(viewsMade, 2);
        lastContext->showPage(pageUrl);
        QVERIFY(clickBack(*placement));
        placement->hide();
        QCOMPARE(pool->hiddenCount(), 1);
        QVERIFY(placement->view());
    }

    void removedItemDropsThePage()
    {
        auto placement = make(QStringLiteral("n1"));
        placement->show();
        lastContext->showPage(pageUrl);
        QPointer<QWidget> page = pageOf(*placement);
        model->removeTask(QStringLiteral("t1"));
        QVERIFY(!placement->view());
        QVERIFY(!placement->headerWidget());
        QVERIFY(showsText(placement.get(), QStringLiteral("Removed")));
        QTRY_VERIFY(page.isNull());
    }

    void destroyedPlacementDropsThePage()
    {
        auto placement = make(QStringLiteral("n1"));
        placement->show();
        lastContext->showPage(pageUrl);
        QPointer<QWidget> page = pageOf(*placement);
        QPointer<QWidget> pageHeader = placement->headerWidget();
        placement->hide();
        QCOMPARE(pool->hiddenCount(), 1);
        placement.reset();
        QCOMPARE(pool->hiddenCount(), 0);
        QVERIFY(page.isNull() && pageHeader.isNull());
    }

    // A placement with no view has nothing to come back to.
    void pageNeedsAView()
    {
        auto placement = make(QStringLiteral("n1"));
        placement->show();
        ItemContext *context = lastContext;
        model->removeTask(QStringLiteral("t1"));
        context->showPage(pageUrl);
        QVERIFY(!pageOf(*placement));
        QVERIFY(!placement->headerWidget());
    }

    // A real page of a data: URL, in a profile of a temporary folder: its
    // toolbar comes after the way back, and it goes with its profile though
    // the view uses none.
    void realPageFollowsItsProfile()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        root.setAutoRemove(false);
        auto profiles = std::make_unique<ProfileManager>(root.path());
        services.profiles = profiles.get();
        {
            // A web view's own toolbar comes back with the view.
            auto web = make(QStringLiteral("p1"));
            web->show();
            QWidget *webToolbar = web->headerWidget();
            QVERIFY(webToolbar);
            lastContext->showPage(pageUrl);
            QVERIFY(web->headerWidget() != webToolbar);
            QVERIFY(clickBack(*web));
            QCOMPARE(web->headerWidget(), webToolbar);
            web.reset();
            viewsMade = 0;

            auto placement = make(QStringLiteral("n1"));
            placement->show();
            QPointer<QWidget> view = placement->view();

            lastContext->showPage(pageUrl);
            auto *panel = qobject_cast<WebPanel *>(pageOf(*placement));
            QVERIFY(panel);
            QCOMPARE(panel->view()->page()->profile(), profiles->profile(kDefaultProfile));
            QCOMPARE(panel->toolbar()->parentWidget(), placement->headerWidget());
            QVERIFY(clickBack(*placement));
            QVERIFY(view->isVisible());
            QTRY_VERIFY(!pageOf(*placement));

            // A change of the item's profile. The view stays, and a change
            // of the list that leaves the item's profile as it is keeps the page.
            lastContext->showPage(pageUrl);
            QPointer<QWidget> page = pageOf(*placement);
            Profile work;
            work.id = QStringLiteral("work");
            work.name = QStringLiteral("Work");
            Profile standard = *model->profile(kDefaultProfile);
            model->setProfiles({standard, work});
            QCOMPARE(pageOf(*placement), page.data());
            Group group = *model->group(QStringLiteral("g1"));
            group.profile = QStringLiteral("work");
            model->setGroup(group);
            QCOMPARE(placement->view(), view.data());
            QVERIFY(view->isVisible());
            QVERIFY(!placement->headerWidget());
            QTRY_VERIFY(page.isNull());

            // Clearing its profile.
            lastContext->showPage(pageUrl);
            page = pageOf(*placement);
            QSignalSpy cleared(profiles.get(), &ProfileManager::profileCleared);
            profiles->clearProfile(QStringLiteral("work"));
            QCOMPARE(placement->view(), view.data());
            QVERIFY(view->isVisible());
            QVERIFY(!placement->headerWidget());
            QTRY_VERIFY(page.isNull());
            QTRY_COMPARE_WITH_TIMEOUT(cleared.count(), 1, 15000);
            QCOMPARE(placement->view(), view.data());
            QCOMPARE(viewsMade, 1);

            // The page goes with the placement, before its profile.
            lastContext->showPage(pageUrl);
        }
        services.profiles = nullptr;
        profiles.reset();
        // Chromium closes some profile files a moment after the profile goes.
        const QString path = root.path();
        QVERIFY(QTest::qWaitFor([&] { return QDir(path).removeRecursively(); }, 15000));
    }

    void addItemToTaskAddsAnItem()
    {
        auto placement = make(QStringLiteral("n1"));
        placement->show();
        QPointer<QWidget> view = placement->view();
        const auto dashboards = model->task(QStringLiteral("t1"))->dashboards.size();
        const auto items = model->task(QStringLiteral("t1"))->items.size();
        const QJsonObject config{{QStringLiteral("text"), QStringLiteral("added")}};

        const QString id = lastContext->addItemToTask(QStringLiteral("test.web"), QStringLiteral("Added"), config);
        QVERIFY(!id.isEmpty());
        QCOMPARE(model->task(QStringLiteral("t1"))->items.size(), items + 1);
        const Item *added = model->item(QStringLiteral("t1"), id);
        QVERIFY(added);
        QCOMPARE(added->type, QStringLiteral("test.web"));
        QCOMPARE(added->name, QStringLiteral("Added"));
        QCOMPARE(added->config, config);
        QVERIFY(added->profile.isEmpty());
        QCOMPARE(model->task(QStringLiteral("t1"))->dashboards.size(), dashboards);

        // Each item has its own id, and the calling view stays.
        QVERIFY(lastContext->addItemToTask(QStringLiteral("test.web"), QStringLiteral("Added"), config) != id);
        QCOMPARE(placement->view(), view.data());
        QCOMPARE(viewsMade, 1);
    }

    void addItemToTaskNeedsTaskAndType()
    {
        const auto items = model->task(QStringLiteral("t1"))->items.size();
        ItemContextImpl context(services, QStringLiteral("t1"), QStringLiteral("n1"));
        QVERIFY(context.addItemToTask(QStringLiteral("nothing.here"), QStringLiteral("A"), {}).isEmpty());
        QCOMPARE(model->task(QStringLiteral("t1"))->items.size(), items);

        ItemContextImpl noTask(services, QStringLiteral("missing"), QStringLiteral("n1"));
        QVERIFY(noTask.addItemToTask(QStringLiteral("test.web"), QStringLiteral("A"), {}).isEmpty());

        PlacementServices noModel;
        ItemContextImpl empty(noModel, QStringLiteral("t1"), QStringLiteral("n1"));
        QVERIFY(empty.addItemToTask(QStringLiteral("test.web"), QStringLiteral("A"), {}).isEmpty());
    }

    void noStatusWithoutCacheOrForOtherTypes()
    {
        auto withoutCache = make(QStringLiteral("s1"));
        QVERIFY(!withoutCache->hasStatus());
        QCOMPARE(withoutCache->cellStatus(), CellStatus());

        enableStatus();
        auto native = make(QStringLiteral("n1"));
        QVERIFY(!native->hasStatus());
        native->show();
        QCOMPARE(native->cellStatus(), CellStatus());
        // Refresh does nothing for a type without status.
        native->refresh();
        QCOMPARE(statusFetches, 0);
    }

    // Each state of the status gives its own cell status.
    void statusReachesTheCell()
    {
        enableStatus();
        nextStatus[QStringLiteral("s1")] = okStatus({reason(QStringLiteral("pipeline-failed"), QStringLiteral("Pipeline failed"))});
        auto placement = make(QStringLiteral("s1"));
        QVERIFY(placement->hasStatus());
        QSignalSpy changed(placement.get(), &Placement::cellStatusChanged);

        // Shown with no status, the item is fetched at once.
        placement->show();
        QCOMPARE(statusFetches, 1);
        CellStatus status = placement->cellStatus();
        QCOMPARE(status.kind, CellStatus::Kind::Attention);
        QCOMPARE(status.count, 1);
        QCOMPARE(status.tooltip, QStringLiteral("Pipeline failed"));
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed.last().at(0).value<CellStatus>(), status);

        // No reasons, nothing to show.
        refreshWith(okStatus());
        QCOMPARE(placement->cellStatus(), CellStatus());

        // Problems that the profile's settings fix.
        refreshWith(failedStatus(ItemStatus::State::NoToken, QStringLiteral("No token is set.")));
        status = placement->cellStatus();
        QCOMPARE(status.kind, CellStatus::Kind::Problem);
        QCOMPARE(status.text, QStringLiteral("Token not set"));
        QVERIFY(status.canFix);
        QVERIFY(status.tooltip.contains(QStringLiteral("No token is set.")));
        QVERIFY(status.tooltip.contains(profiles::nameOf(*model, kDefaultProfile)));
        refreshWith(failedStatus(ItemStatus::State::NoServer, QStringLiteral("No server.")));
        QCOMPARE(placement->cellStatus().text, QStringLiteral("No server set"));
        QVERIFY(placement->cellStatus().canFix);
        refreshWith(failedStatus(ItemStatus::State::Refused, QStringLiteral("Refused.")));
        QCOMPARE(placement->cellStatus().text, QStringLiteral("Token refused"));
        QVERIFY(placement->cellStatus().canFix);

        // The fix opens the settings of the item's module in its profile.
        placement->openSettings();
        QCOMPARE(settingsOpened, QStringList{QStringLiteral("default/stat")});

        // Not found has no fix.
        refreshWith(failedStatus(ItemStatus::State::NotFound, QStringLiteral("Not found (404)")));
        QCOMPARE(placement->cellStatus().kind, CellStatus::Kind::Problem);
        QCOMPARE(placement->cellStatus().text, QStringLiteral("Not found"));
        QVERIFY(!placement->cellStatus().canFix);

        // An error over older data shows its age, and keeps the data.
        ItemStatus old = okStatus({reason(QStringLiteral("pipeline-failed"), QStringLiteral("Pipeline failed"))});
        old.updatedAt = QDateTime::currentDateTimeUtc().addSecs(-12 * 60 - 5);
        refreshWith(old);
        QCOMPARE(placement->cellStatus().kind, CellStatus::Kind::Attention);
        refreshWith(failedStatus(ItemStatus::State::Error, QStringLiteral("Host not found")));
        status = placement->cellStatus();
        QCOMPARE(status.kind, CellStatus::Kind::Stale);
        QCOMPARE(status.text, QStringLiteral("12 min ago"));
        QVERIFY(status.tooltip.contains(QStringLiteral("Host not found")));
        QVERIFY(!status.canFix);

        // Without a way to open the settings there is no fix button.
        services.openServerSettings = nullptr;
        auto other = make(QStringLiteral("s1"));
        refreshWith(failedStatus(ItemStatus::State::NoToken, QStringLiteral("No token is set.")));
        QCOMPARE(other->cellStatus().text, QStringLiteral("Token not set"));
        QVERIFY(!other->cellStatus().canFix);
        other->openSettings();
        QCOMPARE(settingsOpened.size(), 1);
    }

    void errorWithoutDataIsAProblem()
    {
        enableStatus();
        nextStatus[QStringLiteral("s1")] = failedStatus(ItemStatus::State::Error, QStringLiteral("Host not found"));
        auto placement = make(QStringLiteral("s1"));
        placement->show();
        QCOMPARE(placement->cellStatus().kind, CellStatus::Kind::Problem);
        QVERIFY(!placement->cellStatus().canFix);
        QVERIFY(placement->cellStatus().tooltip.contains(QStringLiteral("Host not found")));
    }

    void refreshFetchesAtOnce()
    {
        enableStatus();
        nextStatus[QStringLiteral("s1")] = okStatus();
        auto placement = make(QStringLiteral("s1"));
        placement->show();
        QCOMPARE(statusFetches, 1);
        placement->refresh();
        QCOMPARE(statusFetches, 2);
    }

    // The cache hears of each visible placement, so a hidden item is no
    // longer due.
    void visibilityReachesTheCache()
    {
        enableStatus();
        QDateTime now = QDateTime::currentDateTimeUtc();
        cache->setClock([&now] { return now; });
        const auto pollLater = [&] {
            now = now.addSecs(statusDevice->pollInterval() + 1);
            cache->poll();
        };
        nextStatus[QStringLiteral("s1")] = okStatus();

        auto first = make(QStringLiteral("s1"));
        auto second = make(QStringLiteral("s1"));
        // Not shown, the item is not due.
        pollLater();
        QCOMPARE(statusFetches, 0);

        first->show();
        QCOMPARE(statusFetches, 1);
        pollLater();
        QCOMPARE(statusFetches, 2);

        // Two visible placements need two hides.
        second->show();
        first->hide();
        pollLater();
        QCOMPARE(statusFetches, 3);
        // A placement that goes while visible counts as hidden.
        second.reset();
        pollLater();
        QCOMPARE(statusFetches, 3);

        first->show();
        QCOMPARE(statusFetches, 4);
        first.reset();
        pollLater();
        QCOMPARE(statusFetches, 4);
    }

    // A reason that clears on view stops counting after two seconds visible
    // in the active window, and not while the window is inactive.
    void markSeenAfterTwoSecondsVisible()
    {
        enableStatus();
        // The first status records what is seen: nothing.
        refreshWith(okStatus());
        refreshWith(okStatus({reason(QStringLiteral("approved"), QStringLiteral("Approved"), true),
                              reason(QStringLiteral("pipeline-failed"), QStringLiteral("Pipeline failed"))}));
        QVERIFY(attention->needsAttention(QStringLiteral("s1")));

        auto placement = make(QStringLiteral("s1"));
        QCOMPARE(placement->cellStatus().count, 2);
        QCOMPARE(placement->cellStatus().tooltip, QStringLiteral("Approved\nPipeline failed"));

        // Visible, but another window is active.
        placement->show();
        QWidget other;
        other.resize(100, 100);
        other.show();
        other.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&other));
        QTest::qWait(2500);
        QCOMPARE(placement->cellStatus().count, 2);

        // Active: seen after two seconds. The failed pipeline still counts.
        QElapsedTimer timer;
        timer.start();
        placement->activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(placement.get()));
        QCOMPARE(placement->cellStatus().count, 2);
        QTRY_COMPARE_WITH_TIMEOUT(placement->cellStatus().count, 1, 5000);
        QVERIFY(timer.elapsed() >= 1900);
        QCOMPARE(placement->cellStatus().tooltip, QStringLiteral("Pipeline failed"));
        QCOMPARE(attention->reasons(QStringLiteral("s1")).size(), 1);
    }

    // Status through a real ServerClients over a fake network: a token not
    // set, then a token the server refuses with 401.
    void refusedTokenShowsAsFixableProblem()
    {
        enableStatus();
        statusFromServer = true;
        network->status = 401;
        auto placement = make(QStringLiteral("s1"));
        placement->show();
        QTRY_COMPARE(placement->cellStatus().kind, CellStatus::Kind::Problem);
        QCOMPARE(placement->cellStatus().text, QStringLiteral("Token not set"));
        QVERIFY(placement->cellStatus().canFix);
        QVERIFY(network->sent.isEmpty());

        const QString key = secretKey(QStringLiteral("ws"), kDefaultProfile, QStringLiteral("stat"), QStringLiteral("token"));
        secrets->write(key, QStringLiteral("secret-1"));
        QTRY_COMPARE(secrets->state(key), SecretStore::State::Set);
        placement->refresh();
        QTRY_COMPARE(placement->cellStatus().text, QStringLiteral("Token refused"));
        QCOMPARE(placement->cellStatus().kind, CellStatus::Kind::Problem);
        QVERIFY(placement->cellStatus().canFix);
        QVERIFY(placement->cellStatus().tooltip.contains(QStringLiteral("401")));
        QCOMPARE(network->sent.size(), 1);
        QCOMPARE(network->sent.at(0).url(), QUrl(QStringLiteral("https://stat.example/api/thing")));
        QCOMPARE(network->sent.at(0).rawHeader("PRIVATE-TOKEN"), QByteArray("secret-1"));
        QVERIFY(servers->failing(kDefaultProfile, QStringLiteral("stat")));

        // The fix button opens the module's settings in the item's profile.
        placement->openSettings();
        QCOMPARE(settingsOpened, QStringList{QStringLiteral("default/stat")});
    }

    void contextGivesServerAndStatus()
    {
        {
            // Without the services there is neither.
            ItemContextImpl context(services, QStringLiteral("t1"), QStringLiteral("s1"));
            QVERIFY(!context.server());
            QVERIFY(!context.status());
        }
        enableStatus();
        ItemContextImpl context(services, QStringLiteral("t1"), QStringLiteral("s1"));
        QVERIFY(context.server());
        QCOMPARE(context.server(), servers->client(kDefaultProfile, QStringLiteral("stat")));
        QCOMPARE(context.server()->baseUrl(), QUrl(QStringLiteral("https://stat.example/")));
        QCOMPARE(context.status(), &cache->source(QStringLiteral("s1")));

        // A module without a server, and a type without status.
        ItemContextImpl native(services, QStringLiteral("t1"), QStringLiteral("n1"));
        QVERIFY(!native.server());
        QVERIFY(!native.status());
        ItemContextImpl missing(services, QStringLiteral("t1"), QStringLiteral("nothing"));
        QVERIFY(!missing.server());
        QVERIFY(!missing.status());
    }

    // A change of the item's type removes its status, and tells the frame.
    void typeChangeRemovesStatus()
    {
        enableStatus();
        nextStatus[QStringLiteral("s1")] = okStatus({reason(QStringLiteral("x"), QStringLiteral("X"))});
        auto placement = make(QStringLiteral("s1"));
        placement->show();
        QCOMPARE(placement->cellStatus().kind, CellStatus::Kind::Attention);
        QSignalSpy changed(placement.get(), &Placement::cellStatusChanged);
        changeItem(*model, QStringLiteral("s1"), [](Item &item) { item.type = QStringLiteral("test.native"); });
        QVERIFY(!placement->hasStatus());
        QCOMPARE(placement->cellStatus(), CellStatus());
        QVERIFY(changed.count() >= 1);
    }
    // A change to only the type's view keys keeps the view and tells it. Any
    // other change makes it again. See decision 0081.
    void viewKeyChangeKeepsTheView()
    {
        auto placement = make(QStringLiteral("k1"));
        placement->show();
        QPointer<QWidget> view = placement->view();
        QCOMPARE(viewsMade, 1);

        changeItem(*model, QStringLiteral("k1"), [](Item &item) { item.config[QStringLiteral("note")] = QStringLiteral("b"); });
        QCOMPARE(viewsMade, 1);
        QCOMPARE(placement->view(), view.data());
        QCOMPARE(configsReported.size(), 1);
        QCOMPARE(configsReported.last().value(QStringLiteral("note")).toString(), QStringLiteral("b"));

        // The record follows, so a change back is a change too.
        changeItem(*model, QStringLiteral("k1"), [](Item &item) { item.config[QStringLiteral("note")] = QStringLiteral("a"); });
        QCOMPARE(configsReported.size(), 2);
        QCOMPARE(placement->view(), view.data());

        // Removing the key is a change of the key.
        changeItem(*model, QStringLiteral("k1"), [](Item &item) { item.config.remove(QStringLiteral("note")); });
        QCOMPARE(configsReported.size(), 3);
        QCOMPARE(placement->view(), view.data());

        // Another key makes the view again, and does not report.
        changeItem(*model, QStringLiteral("k1"), [](Item &item) {
            item.config[QStringLiteral("text")] = QStringLiteral("other");
            item.config[QStringLiteral("note")] = QStringLiteral("c");
        });
        QCOMPARE(viewsMade, 2);
        QCOMPARE(configsReported.size(), 3);
        QCOMPARE(viewText(*placement), QStringLiteral("other"));
        QTRY_VERIFY(view.isNull());

        // The old view's receiver is gone; the new view's hears the next change.
        changeItem(*model, QStringLiteral("k1"), [](Item &item) { item.config[QStringLiteral("note")] = QStringLiteral("d"); });
        QCOMPARE(configsReported.size(), 4);
        QCOMPARE(viewsMade, 2);
    }

    // A type without view keys makes the view again for any key.
    void otherTypesHaveNoViewKeys()
    {
        auto placement = make(QStringLiteral("n1"));
        placement->show();
        changeItem(*model, QStringLiteral("n1"), [](Item &item) { item.config[QStringLiteral("note")] = QStringLiteral("x"); });
        QCOMPARE(viewsMade, 2);
        QVERIFY(configsReported.isEmpty());
    }

    // The view writes its own key through the context: the view stays.
    void viewWritesItsOwnKey()
    {
        auto placement = make(QStringLiteral("k1"));
        placement->show();
        QPointer<QWidget> view = placement->view();
        QJsonObject config = lastContext->config();
        config[QStringLiteral("note")] = QStringLiteral("typed");
        lastContext->updateConfig(config);
        QCOMPARE(placement->view(), view.data());
        QCOMPARE(viewsMade, 1);
        QCOMPARE(configsReported.size(), 1);
    }

    // Receivers are called until they are destroyed.
    void configReceiversEndWithTheirObject()
    {
        ItemContextImpl context(services, QStringLiteral("t1"), QStringLiteral("k1"));
        int calls = 0;
        auto receiver = std::make_unique<QObject>();
        context.onConfigChanged(receiver.get(), [&calls](const QJsonObject &) { ++calls; });
        context.onConfigChanged(nullptr, [&calls](const QJsonObject &) { ++calls; });
        context.notifyConfigChanged({});
        QCOMPARE(calls, 1);
        receiver.reset();
        context.notifyConfigChanged({});
        QCOMPARE(calls, 1);
    }

    // A native view is made again when a folder of its project is mapped, at
    // once when the project changes, else when it is shown. A web view keeps
    // its page.
    void folderMappingMakesNativeViewsAgain()
    {
        QTemporaryDir dir;
        QSettings ini(dir.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
        DeviceSettings device(&ini);
        services.settings = &device;
        Project project = *model->project(QStringLiteral("p1"));
        Folder folder;
        folder.id = QStringLiteral("f1");
        folder.name = QStringLiteral("app");
        folder.main = true;
        project.folders.push_back(folder);
        model->setProject(project);

        auto native = make(QStringLiteral("n1"));
        auto web = make(QStringLiteral("w1"));
        native->show();
        web->show();
        QCOMPARE(viewsMade, 2);

        // Mapped as by the project dialog, which also changes the project.
        device.setFolderPath(QStringLiteral("f1"), QStringLiteral("C:/src/app"));
        project.name = QStringLiteral("Renamed");
        model->setProject(project);
        QCOMPARE(viewsMade, 3);

        // Mapped while hidden, as by another item's mapFolder().
        native->hide();
        device.setFolderPath(QStringLiteral("f1"), QStringLiteral("D:/app"));
        native->show();
        QCOMPARE(viewsMade, 4);
        native->hide();
        native->show();
        QCOMPARE(viewsMade, 4);
        services.settings = nullptr;
    }

    // The context's M7 members that read this device: the main folder,
    // device settings and files.
    void contextReadsTheDevice()
    {
        QTemporaryDir dir;
        QSettings ini(dir.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
        DeviceSettings device(&ini);
        services.settings = &device;
        ItemContextImpl context(services, QStringLiteral("t1"), QStringLiteral("n1"));

        // The main folder, for an empty id.
        QVERIFY(context.folderPath(QString()).isEmpty());
        Project project = *model->project(QStringLiteral("p1"));
        Folder other;
        other.id = QStringLiteral("f1");
        Folder main;
        main.id = QStringLiteral("f2");
        main.main = true;
        project.folders = {other, main};
        model->setProject(project);
        device.setFolderPath(QStringLiteral("f1"), QStringLiteral("C:/one"));
        QVERIFY(context.folderPath(QString()).isEmpty());
        device.setFolderPath(QStringLiteral("f2"), QStringLiteral("C:/two"));
        QCOMPARE(context.folderPath(QString()), QStringLiteral("C:/two"));
        QCOMPARE(context.folderPath(QStringLiteral("f1")), QStringLiteral("C:/one"));

        // A device setting of the item's module, else the field's default.
        QCOMPARE(context.deviceSetting(QStringLiteral("editor")), QJsonValue(QStringLiteral("code")));
        QVERIFY(context.deviceSetting(QStringLiteral("lines")).isNull());
        QVERIFY(context.deviceSetting(QStringLiteral("undeclared")).isUndefined());
        device.setModuleSetting(QStringLiteral("test"), QStringLiteral("editor"), QStringLiteral("subl"));
        device.setModuleSetting(QStringLiteral("test"), QStringLiteral("lines"), 4);
        device.setModuleSetting(QStringLiteral("test"), QStringLiteral("undeclared"), 1);
        QCOMPARE(context.deviceSetting(QStringLiteral("editor")), QJsonValue(QStringLiteral("subl")));
        QCOMPARE(context.deviceSetting(QStringLiteral("lines")), QJsonValue(4));
        QVERIFY(context.deviceSetting(QStringLiteral("undeclared")).isUndefined());
        // Another module's item does not see them.
        ItemContextImpl stat(services, QStringLiteral("t1"), QStringLiteral("s1"));
        QVERIFY(stat.deviceSetting(QStringLiteral("editor")).isUndefined());

        // Files: only an existing plain name in files/.
        QVERIFY(context.filePath(QStringLiteral("a.png")).isEmpty());
        Model storeModel;
        Store store(storeModel);
        QTemporaryDir data;
        store.open(data.path());
        const QString name = store.saveFile("png", QStringLiteral("png"));
        QVERIFY(!name.isEmpty());
        services.store = &store;
        ItemContextImpl withFiles(services, QStringLiteral("t1"), QStringLiteral("n1"));
        QCOMPARE(withFiles.filePath(name), QFileInfo(store.filePath(name)).absoluteFilePath());
        QVERIFY(withFiles.filePath(QStringLiteral("missing.png")).isEmpty());
        QVERIFY(withFiles.filePath(QString()).isEmpty());
        QVERIFY(withFiles.filePath(QStringLiteral("../") + name).isEmpty());
        QVERIFY(withFiles.filePath(QStringLiteral("..")).isEmpty());
        QVERIFY(withFiles.filePath(QStringLiteral("sub\\") + name).isEmpty());
        store.close();
        services.store = nullptr;
        services.settings = nullptr;
    }

    // Without a Git reader, gitStatus() never answers; an empty path has no head.
    void gitWithoutReader()
    {
        ItemContextImpl context(services, QStringLiteral("t1"), QStringLiteral("n1"));
        QObject receiver;
        bool called = false;
        context.gitStatus(QDir::tempPath(), &receiver, [&called](const GitState &) { called = true; });
        QCoreApplication::processEvents();
        QVERIFY(!called);
        QVERIFY(context.gitHead(QString()).isEmpty());
    }
};

int main(int argc, char *argv[])
{
    // Qt WebEngine needs shared OpenGL contexts, set before the application.
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    // Without a display, Chromium's GPU process can fail a check when a view
    // goes away. Pages render in software instead.
    if (qEnvironmentVariableIsEmpty("QTWEBENGINE_CHROMIUM_FLAGS"))
        qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--disable-gpu");
    QApplication app(argc, argv);
    TestPlacement test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_placement.moc"
