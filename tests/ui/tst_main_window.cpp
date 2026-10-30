// Tests MainWindow on the fixtures: the root tabs, the preview and Focus slots,
// new root dashboards, closing and reopening tabs, the warning bar, focusing an
// item, views following the model, detached windows, the web views of item
// cells, and the api services: failing servers and keychain errors in the
// warning bar, tokens following a new workspace id, and status polling.
#include "api/ApiServices.h"
#include "api/Attention.h"
#include "api/SecretBackend.h"
#include "api/SecretStore.h"
#include "api/ServerClient.h"
#include "api/StatusCache.h"
#include "core/DeviceSettings.h"
#include "core/GroupColours.h"
#include "core/Model.h"
#include "core/Store.h"
#include "core/Profiles.h"
#include "items/ItemTypeRegistry.h"
#include "items/Placement.h"
#include "items/ViewPool.h"
#include "layout/CellFrame.h"
#include "layout/LayoutView.h"
#include "layout/TaskCell.h"
#include "ui/DashboardPage.h"
#include "ui/DetachedWindow.h"
#include "ui/EntityActions.h"
#include "ui/FieldForm.h"
#include "ui/FocusView.h"
#include "ui/ImagePaste.h"
#include "ui/ImagePasteDialog.h"
#include "ui/MainWindow.h"
#include "ui/RootDashboardsDialog.h"
#include "ui/RootTabBar.h"
#include "ui/SlotTabBar.h"
#include "ui/Sidebar.h"
#include "ui/SidebarDelegate.h"
#include "ui/SettingsDialog.h"
#include "ui/SidebarModel.h"
#include "ui/Shortcuts.h"
#include "ui/ProfilesDialog.h"
#include "web/ProfileManager.h"
#include "web/WebPanel.h"

#include <mirante/ApiClient.h>
#include <mirante/ItemContext.h>
#include <mirante/ItemType.h>
#include <mirante/ModuleSettings.h>
#include <mirante/Status.h>

#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QDirIterator>
#include <QEnterEvent>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QScreen>
#include <QSettings>
#include <QSignalSpy>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include <QTimer>
#include <QTreeView>
#include <QWebEngineView>

#include <memory>

using namespace mirante;

namespace {

const QString kMorning = QStringLiteral("60139a76-dd45-50ea-890b-ae022026e421");
const QString kRelease = QStringLiteral("5247a9e5-ee54-5552-8831-0b2ff361afba");
const QString kFixLoginTask = QStringLiteral("760c8e20-8555-5a2c-bcbe-75e7fd08ca74");
const QString kFixLoginDashboard = QStringLiteral("e7c480bf-fc19-50f3-81e4-18fc51f27f6c");
const QString kOverviewTask = QStringLiteral("c34def7a-af34-507f-a44d-770bb60dbe95");
const QString kReviewsItem = QStringLiteral("191057b6-8355-5994-beae-62a500d35f8d");
// A web.page item of the Accessibility audit task.
const QString kAuditTask = QStringLiteral("2038cae6-f1d0-5380-a038-ecc1602c4c5a");
const QString kAuditPage = QStringLiteral("161e69b0-d18e-595b-8a31-08f457e40d3b");

// The page every test web item shows, in place of the fixture's address, so
// no test uses the network.
const QUrl kTestPage(QStringLiteral("data:text/html,<p>test page</p>"));

// A stand-in for the web module's web.page, which the ui tests do not link.
class TestWebPage : public ItemType
{
public:
    QString id() const override { return QStringLiteral("web.page"); }
    QString name() const override { return QStringLiteral("Web page"); }
    QIcon icon() const override { return {}; }
    Rendering rendering() const override { return Rendering::Web; }
    QWidget *createView(ItemContext &context, QWidget *parent) const override
    {
        return context.createWebView(kTestPage, parent);
    }
};

void registerTestWeb(ModuleRegistrar &registrar) { registrar.addItemType(std::make_unique<TestWebPage>()); }

// The Overview task has an image item, Garden sketch.
const QString kImageTask = QStringLiteral("d71f3227-79a2-55e9-a0d8-429b99432282");
const QString kGardenSketch = QStringLiteral("734a1531-d2f6-5ef8-9585-4233708ba49f");

// A stand-in for the image module's image type, which the ui tests do not link.
class TestImage : public ItemType
{
public:
    QString id() const override { return QStringLiteral("image"); }
    QString name() const override { return QStringLiteral("Image"); }
    QIcon icon() const override { return {}; }
    QString pastedImageField() const override { return QStringLiteral("file"); }
};

void registerTestImage(ModuleRegistrar &registrar) { registrar.addItemType(std::make_unique<TestImage>()); }

// The paste dialog's exec: crops to crop and accepts. Any other dialog is refused.
std::function<bool(QDialog &)> cropTo(const QRect &crop)
{
    return [crop](QDialog &d) {
        auto *dialog = qobject_cast<ImagePasteDialog *>(&d);
        if (!dialog)
            return false;
        dialog->setCrop(crop);
        return true;
    };
}

// The registry has no modules, so every item has an unknown type.
struct Fixture;
// The colour cellstyle::tintHeader gives a title bar for colour, as a name.
QString tinted(const QWidget &page, const QColor &colour)
{
    const QColor window = page.palette().color(QPalette::Window);
    const qreal amount = 0.22;
    return QColor::fromRgbF(float(window.redF() + (colour.redF() - window.redF()) * amount),
                            float(window.greenF() + (colour.greenF() - window.greenF()) * amount),
                            float(window.blueF() + (colour.blueF() - window.blueF()) * amount))
        .name();
}

QString overviewTitle(const Fixture &f);

// Copies the fixtures into a folder, so a test can open them as a workspace
// that is written to.
bool copyFixtures(const QString &target)
{
    QDirIterator files(QStringLiteral(MIRANTE_FIXTURES_DIR), QDir::Files, QDirIterator::Subdirectories);
    while (files.hasNext()) {
        const QString source = files.next();
        const QString relative = QDir(QStringLiteral(MIRANTE_FIXTURES_DIR)).relativeFilePath(source);
        if (!QDir(target).mkpath(QFileInfo(relative).path()) || !QFile::copy(source, QDir(target).filePath(relative)))
            return false;
    }
    return true;
}

// The window starts on the fixtures, loaded without the Store, so nothing is
// written until a test opens a workspace. The settings live in a temporary
// INI file. With web set, item cells get real views: web.page items show a
// data: page, in profiles of a temporary folder.
struct Fixture
{
    QTemporaryDir settingsDir;
    QSettings iniSettings{settingsDir.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat};
    DeviceSettings settings{&iniSettings};
    ItemTypeRegistry types;
    Model model;
    Store dataStore{model};
    QString profilesRoot;
    std::unique_ptr<ProfileManager> profiles;
    std::unique_ptr<ViewPool> pool;
    std::unique_ptr<MainWindow> window;

    explicit Fixture(bool web = false)
    {
        const auto problems = store::load(QStringLiteral(MIRANTE_FIXTURES_DIR), model);
        if (!problems.empty())
            qWarning() << "fixture problems:" << problems.size();
        if (web) {
            types.registerModule(QStringLiteral("web"), &registerTestWeb);
            QTemporaryDir root;
            root.setAutoRemove(false);
            profilesRoot = root.path();
            profiles = std::make_unique<ProfileManager>(profilesRoot);
            pool = std::make_unique<ViewPool>();
        }
        window = std::make_unique<MainWindow>(model, types, dataStore, settings, profiles.get(), pool.get());
    }

    ~Fixture()
    {
        // The pages go before their profiles. Chromium closes some profile
        // files a moment after the profile goes.
        window.reset();
        profiles.reset();
        if (!profilesRoot.isEmpty())
            (void)QTest::qWaitFor([this] { return QDir(profilesRoot).removeRecursively(); }, 15000);
    }

    QWidget *warningBar() const { return window->findChild<QWidget *>(QStringLiteral("warningBar")); }
    QString warningText() const
    {
        const auto labels = warningBar()->findChildren<QLabel *>();
        return labels.isEmpty() ? QString() : labels.front()->text();
    }

    RootTabBar *bar() const { return window->rootTabBar(); }
    QString tabText(int index) const { return bar()->tabText(index); }
    SlotTabBar *slotBar() const { return window->slotTabBar(); }
    QString previewText() const { return slotBar()->tabText(SlotTabBar::PreviewSlot); }

    // The group of a task.
    QString groupOf(const QString &taskId) const
    {
        const Task *task = model.task(taskId);
        const Project *project = task ? model.project(task->projectId) : nullptr;
        return project ? project->groupId : QString();
    }
};

// The first item cell in a widget, or the one showing itemId.
CellFrame *itemCell(const QWidget *within, const QString &itemId = QString())
{
    for (CellFrame *frame : within->findChildren<CellFrame *>()) {
        const auto ref = frame->ref();
        if (ref && ref->isItem() && (itemId.isEmpty() || ref->item == itemId))
            return frame;
    }
    return nullptr;
}

// Opens a view's cell menu, and chooses the entry named choose if there is one.
void chooseInCellMenu(LayoutView &view, const NodePath &path, const QString &choose)
{
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, [choose] {
        auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        if (!menu)
            return;
        for (QAction *action : menu->actions()) {
            if (action->text() == choose) {
                menu->setActiveAction(action);
                QTest::keyClick(menu, Qt::Key_Return);
                return;
            }
        }
        menu->close();
    });
    timer.start(0);
    emit view.cellMenuRequested(path, view.mapToGlobal(QPoint(10, 10)));
}

// Records the warnings Qt prints while it exists.
struct WarningLog
{
    static inline QStringList messages;
    QtMessageHandler previous = nullptr;

    WarningLog()
    {
        messages.clear();
        previous = qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &, const QString &message) {
            if (type == QtWarningMsg || type == QtCriticalMsg)
                messages << message;
        });
    }
    ~WarningLog() { qInstallMessageHandler(previous); }
};

// A module with a server, as gitlab and jira have: a screen with a base URL
// and a token, and an item type whose status has one attention reason.
class SrvServerType : public ServerType
{
public:
    QString screenId() const override { return QStringLiteral("general"); }
    QString tokenKey() const override { return QStringLiteral("token"); }
    QUrl baseUrl(const QJsonObject &values) const override
    {
        return QUrl(values.value(QStringLiteral("baseUrl")).toString());
    }
    void authorize(QNetworkRequest &request, const QString &token, const QJsonObject &) const override
    {
        request.setRawHeader("X-Token", token.toUtf8());
    }
};

class SrvScreen : public SettingsScreen
{
public:
    QString id() const override { return QStringLiteral("general"); }
    QString name() const override { return QStringLiteral("General Configuration"); }
    std::vector<Field> fields() const override
    {
        Field base;
        base.key = QStringLiteral("baseUrl");
        base.label = QStringLiteral("Base URL");
        base.type = Field::Type::Url;
        Field token;
        token.key = QStringLiteral("token");
        token.label = QStringLiteral("Token");
        token.type = Field::Type::Secret;
        return {base, token};
    }
};

class SrvThing : public ItemType
{
public:
    QString id() const override { return QStringLiteral("srv.thing"); }
    QString name() const override { return QStringLiteral("Thing"); }
    QIcon icon() const override { return {}; }
    bool hasStatus() const override { return true; }
    void fetchStatus(StatusJob &job) const override
    {
        ++fetches;
        ItemStatus status;
        status.state = ItemStatus::State::Ok;
        AttentionReason reason;
        reason.key = QStringLiteral("failed");
        reason.text = QStringLiteral("Pipeline failed");
        status.attention << reason;
        job.finish(status);
    }
    static inline int fetches = 0;
};

void registerSrv(ModuleRegistrar &registrar)
{
    registrar.setModuleName(QStringLiteral("Srv"));
    registrar.setServerType(std::make_unique<SrvServerType>());
    registrar.addSettingsScreen(std::make_unique<SrvScreen>());
    registrar.addItemType(std::make_unique<SrvThing>());
}

// A reply that ends at once with an HTTP status and no body.
class StatusReply : public QNetworkReply
{
public:
    StatusReply(QNetworkAccessManager::Operation op, const QNetworkRequest &request, int status, QObject *parent)
        : QNetworkReply(parent)
    {
        setOperation(op);
        setRequest(request);
        setUrl(request.url());
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        QTimer::singleShot(0, this, [this, status] {
            setAttribute(QNetworkRequest::HttpStatusCodeAttribute, status);
            if (status >= 400)
                setError(QNetworkReply::AuthenticationRequiredError, QStringLiteral("HTTP %1").arg(status));
            setFinished(true);
            emit finished();
        });
    }
    void abort() override {}
    qint64 bytesAvailable() const override { return 0; }

protected:
    qint64 readData(char *, qint64) override { return 0; }
};

// Answers every request with 401, as a server that refuses the token. No
// test uses the network.
class RefusingNetwork : public QNetworkAccessManager
{
public:
    int requests = 0;

protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &request, QIODevice *) override
    {
        ++requests;
        return new StatusReply(op, request, 401, this);
    }
};

// A keychain that refuses every call, as a locked one does.
class LockedBackend : public SecretBackend
{
public:
    void read(const QString &, std::function<void(std::optional<QString>, const QString &)> done) override
    {
        QTimer::singleShot(0, [done] { done(std::nullopt, QStringLiteral("The keychain is locked")); });
    }
    void write(const QString &, const QString &, std::function<void(const QString &)> done) override
    {
        QTimer::singleShot(0, [done] { done(QStringLiteral("The keychain is locked")); });
    }
    void remove(const QString &, std::function<void(const QString &)> done) override
    {
        QTimer::singleShot(0, [done] { done(QStringLiteral("The keychain is locked")); });
    }
};

// The api services over a fixture's window, with the srv module, tokens in
// memory and a network that refuses every token.
struct Api
{
    SecretStore secrets;
    RefusingNetwork network;
    ServerClients servers;
    StatusCache status;
    Attention attention;

    Api(Fixture &f, std::unique_ptr<SecretBackend> backend = makeMemoryBackend());
};

const QString kSampleWorkspaceId = QStringLiteral("5a3e9c1d-7b2f-5e48-a6d0-3c8f1b2e4d97");

QString srvTokenKey(const QString &workspaceId)
{
    return secretKey(workspaceId, QStringLiteral("default"), QStringLiteral("srv"), QStringLiteral("token"));
}

// The tab title of the Overview task's default dashboard: "Task / Dashboard".
QString overviewTitle(const Fixture &f)
{
    const Task *task = f.model.task(kOverviewTask);
    return QStringLiteral("%1 / %2").arg(task->title, f.model.taskDashboard(kOverviewTask, task->defaultDashboardId)->name);
}

Api::Api(Fixture &f, std::unique_ptr<SecretBackend> backend)
    : secrets(std::move(backend))
    , servers(f.model, f.types, secrets, &network)
    , status(f.model, f.types, servers, f.settings)
    , attention(f.model, status, f.settings)
{
    f.types.registerModule(QStringLiteral("srv"), &registerSrv);
    Model *model = &f.model;
    f.types.setSettingsReader([model](const QString &profileId, const QString &moduleId, const QString &screenId) {
        return model->moduleSettings(profileId, moduleId, screenId);
    });
    ApiServices services;
    services.secrets = &secrets;
    services.servers = &servers;
    services.status = &status;
    services.attention = &attention;
    f.window->setApiServices(services);
}

} // namespace

class TestMainWindow : public QObject
{
    Q_OBJECT

private slots:
    void rootTabsAtStart()
    {
        Fixture f;
        QCOMPARE(f.bar()->count(), 2);
        QCOMPARE(f.tabText(0), QStringLiteral("Morning"));
        QCOMPARE(f.tabText(1), QStringLiteral("Release"));
        QVERIFY(!f.slotBar()->hasPreview());
        QCOMPARE(f.slotBar()->currentSlot(), -1);
        QVERIFY(f.bar()->selectionShown());
        QCOMPARE(f.bar()->currentIndex(), 0);
        QVERIFY(f.window->currentLayoutView());
        QCOMPARE(f.window->currentLayoutView()->tree().kind, LayoutNode::Kind::Split);
        QVERIFY(f.window->contentArea());
        QVERIFY(f.window->shortcuts());
    }

    // The preview opens in its slot right of the tabs, not as a tab
    // (decision 0048).
    void previewSlot()
    {
        Fixture f;
        f.window->openTaskPreview(kFixLoginTask);
        QCOMPARE(f.bar()->count(), 2);
        QVERIFY(f.slotBar()->hasPreview());
        QCOMPARE(f.slotBar()->currentSlot(), int(SlotTabBar::PreviewSlot));
        QVERIFY(!f.bar()->selectionShown());
        QCOMPARE(f.previewText(), QStringLiteral("Fix login timeout / Work"));
        QVERIFY(!f.slotBar()->tabIcon(SlotTabBar::PreviewSlot).isNull());
        // The preview has a close button and no pin button (decision 0052).
        const auto closeSide = QTabBar::ButtonPosition(
            f.slotBar()->style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, nullptr, f.slotBar()));
        const auto otherSide = closeSide == QTabBar::LeftSide ? QTabBar::RightSide : QTabBar::LeftSide;
        QVERIFY(f.slotBar()->tabButton(SlotTabBar::PreviewSlot, closeSide));
        QVERIFY(!f.slotBar()->tabButton(SlotTabBar::PreviewSlot, otherSide));
        QVERIFY(!f.window->findChild<QToolButton *>(QStringLiteral("pinButton")));
        f.window->show();
        QVERIFY(QTest::qWaitForWindowExposed(f.window.get()));
        QTRY_VERIFY(f.slotBar()->height() > 0);
        QVERIFY(!f.slotBar()->grab().isNull());     // paints the italic slot
        QVERIFY(!f.bar()->grab().isNull());         // paints no selection
        // The slots are on the right of the tabs.
        QVERIFY(f.slotBar()->mapTo(f.window.get(), QPoint()).x() > f.bar()->mapTo(f.window.get(), QPoint()).x());

        const LayoutNode *expected = f.model.layout(DashboardRef{kFixLoginTask, kFixLoginDashboard});
        QVERIFY(expected);
        QVERIFY(layout::sameShape(f.window->currentLayoutView()->tree(), *expected));

        // A second preview replaces the first.
        f.window->openTaskPreview(kOverviewTask);
        QCOMPARE(f.bar()->count(), 2);
        QCOMPARE(f.previewText(), overviewTitle(f));

        // Closing the preview hides its slot, and the root tab is current again.
        f.window->closeCurrentTab();
        QCOMPARE(f.bar()->count(), 2);
        QVERIFY(!f.slotBar()->hasPreview());
        QCOMPARE(f.slotBar()->currentSlot(), -1);
        QVERIFY(f.bar()->selectionShown());
        QCOMPARE(f.window->currentLayoutView()->tree().kind, LayoutNode::Kind::Split);
    }

    // Clicks on the tabs and the slots move the one current page between the
    // two bars.
    void clickingTabsAndSlots()
    {
        Fixture f;
        f.window->show();
        QVERIFY(QTest::qWaitForWindowExposed(f.window.get()));
        f.window->openTaskPreview(kFixLoginTask);
        QTRY_VERIFY(f.slotBar()->tabRect(SlotTabBar::PreviewSlot).width() > 0);
        LayoutView *preview = f.window->currentLayoutView();

        // The current root tab, clicked again, comes back.
        QTest::mouseClick(f.bar(), Qt::LeftButton, {}, f.bar()->tabRect(0).center());
        QCOMPARE(f.slotBar()->currentSlot(), -1);
        QVERIFY(f.bar()->selectionShown());
        QVERIFY(f.window->currentLayoutView() != preview);

        QTest::mouseClick(f.slotBar(), Qt::LeftButton, {}, f.slotBar()->tabRect(SlotTabBar::PreviewSlot).center());
        QCOMPARE(f.slotBar()->currentSlot(), int(SlotTabBar::PreviewSlot));
        QCOMPARE(f.window->currentLayoutView(), preview);

        QTest::mouseClick(f.slotBar(), Qt::LeftButton, {}, f.slotBar()->tabRect(SlotTabBar::FocusSlot).center());
        QCOMPARE(f.slotBar()->currentSlot(), int(SlotTabBar::FocusSlot));
        QVERIFY(!f.window->currentLayoutView());
        QVERIFY(f.window->findChild<FocusView *>()->isVisible());

        // Another root tab.
        QTest::mouseClick(f.bar(), Qt::LeftButton, {}, f.bar()->tabRect(1).center());
        QCOMPARE(f.slotBar()->currentSlot(), -1);
        QCOMPARE(f.bar()->currentIndex(), 1);

        // A middle click closes the preview.
        QTest::mouseClick(f.slotBar(), Qt::MiddleButton, {}, f.slotBar()->tabRect(SlotTabBar::PreviewSlot).center());
        QVERIFY(!f.slotBar()->hasPreview());
        QCOMPARE(f.bar()->currentIndex(), 1);
    }

    void previewOfAChosenDashboard()
    {
        Fixture f;
        // A chosen dashboard replaces the default one in the same preview slot.
        const QString review = QStringLiteral("ece7e850-6e0f-51ef-ad23-4ee948afdd66");
        f.window->openTaskPreview(kFixLoginTask);
        f.window->openTaskPreview(kFixLoginTask, review);
        QCOMPARE(f.bar()->count(), 2);
        QVERIFY(f.slotBar()->hasPreview());
        const LayoutNode *reviewTree = f.model.layout(DashboardRef{kFixLoginTask, review});
        QVERIFY(reviewTree);
        QVERIFY(layout::sameShape(f.window->currentLayoutView()->tree(), *reviewTree));
        f.window->openTaskPreview(kFixLoginTask);
        QVERIFY(f.slotBar()->hasPreview());
        QCOMPARE(f.bar()->count(), 2);
    }

    // Opening the task dashboard that the preview shows switches to the
    // preview. Only one preview exists, and it never becomes a tab.
    void previewIsTheOnlyTaskPage()
    {
        Fixture f;
        f.window->openTaskPreview(kFixLoginTask);
        LayoutView *preview = f.window->currentLayoutView();
        f.window->openRootDashboard(kRelease);
        QCOMPARE(f.bar()->currentIndex(), 1);
        f.window->openTaskPreview(kFixLoginTask);
        QCOMPARE(f.slotBar()->currentSlot(), int(SlotTabBar::PreviewSlot));
        QCOMPARE(f.window->currentLayoutView(), preview);
        QCOMPARE(f.bar()->count(), 2);
        QCOMPARE(int(f.model.rootDashboards().size()), 2);

        // The next preview replaces it.
        f.window->openTaskPreview(kOverviewTask);
        QCOMPARE(f.bar()->count(), 2);
        QCOMPARE(f.previewText(), overviewTitle(f));
    }

    // A task dashboard preview has its own lock in its title bar. The lock at the
    // end of the tab bar row is for root dashboards only.
    void taskTabHasItsOwnLock()
    {
        Fixture f;
        f.window->show();
        QVERIFY(QTest::qWaitForWindowExposed(f.window.get()));
        QToolButton *rootLock = nullptr;
        for (QToolButton *button : f.window->findChildren<QToolButton *>(QStringLiteral("lockButton")))
            rootLock = button;
        QVERIFY(rootLock);
        QVERIFY(rootLock->isVisible());

        f.window->openTaskPreview(kFixLoginTask);
        QVERIFY(!rootLock->isVisible());

        // The sidebar selects the dashboard on screen, under its task.
        auto *tree = f.window->findChild<Sidebar *>()->findChild<QTreeView *>();
        QVERIFY(tree);
        QCOMPARE(tree->currentIndex().data(SidebarModel::IdRole).toString(), kFixLoginDashboard);
        QCOMPARE(tree->currentIndex().data(SidebarModel::TaskIdRole).toString(), kFixLoginTask);
        QWidget *page = f.window->currentLayoutView()->parentWidget();
        auto *taskLock = page->findChild<QToolButton *>(QStringLiteral("taskLockButton"));
        QVERIFY(taskLock);
        QVERIFY(taskLock->isVisible());
        QVERIFY(!f.window->currentLayoutView()->editMode());
        taskLock->click();
        QVERIFY(f.window->currentLayoutView()->editMode());

        f.window->openRootDashboard(kMorning);
        QVERIFY(rootLock->isVisible());
        QVERIFY(!f.window->currentLayoutView()->editMode());
    }

    // The title bar of a task preview is tinted with the task's group colour, and
    // follows a new colour and a move to another group at once.
    void taskTitleBarTintFollowsGroup()
    {
        Fixture f;
        f.window->openTaskPreview(kFixLoginTask);
        QWidget *page = f.window->currentLayoutView()->parentWidget();
        auto *header = page->findChild<QWidget *>(QStringLiteral("taskDashboardHeader"));
        QVERIFY(header);
        const auto headerColour = [&] { return header->palette().color(QPalette::Window).name(); };
        QCOMPARE(headerColour(), tinted(*page, QColor(groupcolours::ofTask(f.model, kFixLoginTask))));

        Group group = *f.model.group(f.groupOf(kFixLoginTask));
        group.colour = QStringLiteral("#ff0000");
        f.model.setGroup(group);
        QCOMPARE(headerColour(), tinted(*page, Qt::red));

        // The task moves to a project of the Personal group, which stores #22c55e.
        Task moved = *f.model.task(kFixLoginTask);
        moved.projectId = QStringLiteral("d7740958-1656-52ae-bb9d-6b6285694e19");
        f.model.setTask(moved);
        QCOMPARE(headerColour(), tinted(*page, QColor(QStringLiteral("#22c55e"))));
    }

    // Editing the previewed task dashboard keeps it in the preview slot. It
    // does not become a tab (decision 0052).
    void editingPreviewKeepsIt()
    {
        Fixture f;
        f.window->openTaskPreview(kFixLoginTask);
        QVERIFY(f.slotBar()->hasPreview());

        const DashboardRef ref{kFixLoginTask, kFixLoginDashboard};
        const LayoutNode *tree = f.model.layout(ref);
        QVERIFY(tree);
        const LayoutNode edited = layout::splitCell(*tree, {}, Edge::Right, std::nullopt).tree;
        f.model.setLayout(ref, edited);
        QTest::qWait(20);

        QVERIFY(f.slotBar()->hasPreview());
        QCOMPARE(f.slotBar()->currentSlot(), int(SlotTabBar::PreviewSlot));
        QCOMPARE(f.bar()->count(), 2);
        QCOMPARE(int(f.model.rootDashboards().size()), 2);
        QVERIFY(layout::sameShape(f.window->currentLayoutView()->tree(), *f.model.layout(ref)));
    }

    void newDashboardOpensInPreview()
    {
        Fixture f;
        const QString review = QStringLiteral("ece7e850-6e0f-51ef-ad23-4ee948afdd66");
        emit f.window->entityActions()->dashboardCreated(kFixLoginTask, review);
        QVERIFY(f.slotBar()->hasPreview());
        QCOMPARE(f.slotBar()->currentSlot(), int(SlotTabBar::PreviewSlot));
        const LayoutNode *reviewTree = f.model.layout(DashboardRef{kFixLoginTask, review});
        QVERIFY(reviewTree);
        QVERIFY(layout::sameShape(f.window->currentLayoutView()->tree(), *reviewTree));
    }

    void otherTaskChangesKeepPreview()
    {
        Fixture f;
        f.window->openTaskPreview(kFixLoginTask);
        QVERIFY(f.slotBar()->hasPreview());
        // The tooltip names the project, which the title leaves out.
        QCOMPARE(f.slotBar()->tabToolTip(SlotTabBar::PreviewSlot), QStringLiteral("backend / Fix login timeout / Work"));

        // A change to the task that leaves this dashboard's layout as it is,
        // such as a new title or a new dashboard, keeps the preview.
        Task task = *f.model.task(kFixLoginTask);
        task.title = QStringLiteral("Login timeout");
        f.model.setTask(task);
        QTest::qWait(20);
        QVERIFY(f.slotBar()->hasPreview());
        QCOMPARE(f.previewText(), QStringLiteral("Login timeout / Work"));
        QCOMPARE(f.slotBar()->tabToolTip(SlotTabBar::PreviewSlot), QStringLiteral("backend / Login timeout / Work"));
    }

    // View > Full Screen uses the platform's key, and leaving full screen
    // brings back the window state from before.
    void fullScreen()
    {
        Fixture f;
        QAction *action = nullptr;
        for (QAction *candidate : f.window->findChildren<QAction *>()) {
            if (candidate->text() == QStringLiteral("&Full Screen"))
                action = candidate;
        }
        QVERIFY(action);
        QCOMPARE(action->shortcut(), QKeySequence(QKeySequence::FullScreen));
        QVERIFY(action->isCheckable());

        f.window->show();
        QVERIFY(QTest::qWaitForWindowExposed(f.window.get()));
        f.window->showMaximized();
        action->trigger();
        QTRY_VERIFY(f.window->windowState().testFlag(Qt::WindowFullScreen));
        QVERIFY(action->isChecked());
        // The menu bar hides, but the menus' shortcuts still work: the key
        // leaves full screen.
        QTRY_VERIFY(!f.window->menuBar()->isVisible());
        QVERIFY(f.window->actions().contains(action));
        QTest::keySequence(f.window.get(), action->shortcut());
        QTRY_VERIFY(!f.window->windowState().testFlag(Qt::WindowFullScreen));
        QVERIFY(f.window->windowState().testFlag(Qt::WindowMaximized));
        QVERIFY(!action->isChecked());
        QVERIFY(f.window->menuBar()->isVisible());
    }

    // The window's place and size are saved on close. The next window opens
    // there while it is visible on a screen, and centred on the primary
    // screen when it is not, such as on a monitor that was unplugged.
    void windowPlace()
    {
        Fixture f;
        const QRect available = QGuiApplication::primaryScreen()->availableGeometry();
        // The smallest window, with room for the title bar above it.
        const QSize size = f.window->minimumSize();
        QVERIFY(available.width() >= size.width() + 20 && available.height() >= size.height() + 60);
        const QRect visible(QPoint(available.left() + 20, available.top() + 60), size);
        f.window->setGeometry(visible);
        f.window->close();
        QCOMPARE(f.settings.windowGeometry(), visible);
        QCOMPARE(f.settings.windowSize(), visible.size());

        f.window = std::make_unique<MainWindow>(f.model, f.types, f.dataStore, f.settings);
        QCOMPARE(f.window->geometry(), visible);

        // A place on no screen is not used; the saved size is.
        f.settings.setWindowGeometry(QRect(QPoint(available.right() + 5000, 100), size));
        f.window = std::make_unique<MainWindow>(f.model, f.types, f.dataStore, f.settings);
        QCOMPARE(f.window->geometry().size(), size);
        QVERIFY(available.contains(f.window->geometry()));
    }

    // The Focus slot is always there, with an icon, and cannot be closed.
    void focusSlot()
    {
        Fixture f;
        QVERIFY(f.slotBar()->isTabVisible(SlotTabBar::FocusSlot));
        QVERIFY(!f.slotBar()->tabIcon(SlotTabBar::FocusSlot).isNull());
        QCOMPARE(f.slotBar()->tabText(SlotTabBar::FocusSlot), QStringLiteral("Focus"));
        const auto closeSide = QTabBar::ButtonPosition(
            f.slotBar()->style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, nullptr, f.slotBar()));
        QVERIFY(!f.slotBar()->tabButton(SlotTabBar::FocusSlot, closeSide));
        QVERIFY(f.slotBar()->tabButton(SlotTabBar::PreviewSlot, closeSide));

        f.window->openFocus();
        QCOMPARE(f.bar()->count(), 2);      // no Focus tab
        QCOMPARE(f.slotBar()->currentSlot(), int(SlotTabBar::FocusSlot));
        f.window->closeCurrentTab();
        QCOMPARE(f.slotBar()->currentSlot(), int(SlotTabBar::FocusSlot));
        QVERIFY(f.window->findChild<FocusView *>());
    }

    void newRootDashboard()
    {
        Fixture f;
        f.window->openTaskPreview(kFixLoginTask);
        f.window->newRootDashboard();
        QCOMPARE(int(f.model.rootDashboards().size()), 3);
        QCOMPARE(f.bar()->count(), 3);
        QCOMPARE(f.bar()->currentIndex(), 2);
        QCOMPARE(f.slotBar()->currentSlot(), -1);
        QVERIFY(f.slotBar()->hasPreview());     // the preview stays in its slot
        QCOMPARE(f.tabText(2), QStringLiteral("New dashboard"));

        LayoutView *view = f.window->currentLayoutView();
        QVERIFY(view->editMode());
        QCOMPARE(view->tree().kind, LayoutNode::Kind::Cell);
        QVERIFY(!view->tree().ref);

        // The + button asks for one too.
        auto *add = f.window->findChild<QToolButton *>(QStringLiteral("newDashboardButton"));
        QVERIFY(add);
        add->click();
        QCOMPARE(int(f.model.rootDashboards().size()), 4);
    }

    void closeAndReopen()
    {
        Fixture f;
        QCOMPARE(f.bar()->currentIndex(), 0);
        f.window->closeCurrentTab();
        QCOMPARE(f.bar()->count(), 1);
        QCOMPARE(f.tabText(0), QStringLiteral("Release"));
        QCOMPARE(int(f.model.rootDashboards().size()), 2);

        f.window->openRootDashboard(kMorning);
        QCOMPARE(f.bar()->count(), 2);
        QCOMPARE(f.tabText(0), QStringLiteral("Morning"));
        QCOMPARE(f.bar()->currentIndex(), 0);

        // The tab's close button does the same.
        emit f.bar()->tabCloseRequested(1);
        QCOMPARE(f.bar()->count(), 1);
        QCOMPARE(f.tabText(0), QStringLiteral("Morning"));
    }

    void warningBar()
    {
        Fixture f;
        auto *bar = f.window->findChild<QFrame *>(QStringLiteral("warningBar"));
        QVERIFY(bar);
        QVERIFY(!bar->isVisibleTo(f.window.get()));

        f.window->setLoadProblems({LoadProblem{QStringLiteral("tasks/broken.json"), QStringLiteral("Not JSON")}});
        QVERIFY(bar->isVisibleTo(f.window.get()));
        const auto labels = bar->findChildren<QLabel *>();
        QVERIFY(!labels.isEmpty());
        QVERIFY(labels.front()->text().contains(QStringLiteral("tasks/broken.json")));

        f.window->setLoadProblems({});
        QVERIFY(!bar->isVisibleTo(f.window.get()));
    }

    void viewsFollowModel()
    {
        Fixture f;
        LayoutView *morning = f.window->currentLayoutView();
        f.model.setRootDashboardLayout(kMorning, layout::emptyLayout());
        QCOMPARE(morning->tree().kind, LayoutNode::Kind::Cell);
        QVERIFY(!morning->tree().ref);

        f.window->openTaskPreview(kFixLoginTask);
        LayoutView *preview = f.window->currentLayoutView();
        QVERIFY(preview != morning);
        f.model.setTaskDashboardLayout(kFixLoginTask, kFixLoginDashboard, layout::emptyLayout());
        QCOMPARE(preview->tree().kind, LayoutNode::Kind::Cell);
    }

    void sidebarCollapse()
    {
        Fixture f;
        auto *sidebar = f.window->findChild<Sidebar *>();
        auto *show = f.window->findChild<QToolButton *>(QStringLiteral("showSidebarButton"));
        QVERIFY(sidebar && show);
        QVERIFY(sidebar->isVisibleTo(f.window.get()));
        emit sidebar->collapseRequested();
        QVERIFY(!sidebar->isVisibleTo(f.window.get()));
        QVERIFY(show->isVisibleTo(f.window.get()));
        show->click();
        QVERIFY(sidebar->isVisibleTo(f.window.get()));
        QVERIFY(!show->isVisibleTo(f.window.get()));
    }

    // The View menu's keys: Ctrl+B shows or hides the sidebar, and
    // Ctrl+Shift+E shows it and focuses its tree on a row. Ctrl+D lists the
    // dashboards, Ctrl+Shift+D makes one, and Ctrl+P manages the profiles.
    void viewShortcuts()
    {
        Fixture f;
        const auto action = [&f](const char *name) {
            return f.window->findChild<QAction *>(QString::fromLatin1(name));
        };
        QCOMPARE(action("showSidebar")->shortcut(), QKeySequence(Qt::CTRL | Qt::Key_B));
        QCOMPARE(action("focusSidebar")->shortcut(), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E));
        QCOMPARE(action("allDashboards")->shortcut(), QKeySequence(Qt::CTRL | Qt::Key_D));
        QCOMPARE(action("newDashboard")->shortcut(), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D));
        QCOMPARE(action("manageProfiles")->shortcut(), QKeySequence(Qt::CTRL | Qt::Key_P));

        auto *sidebar = f.window->findChild<Sidebar *>();
        action("showSidebar")->trigger();
        QVERIFY(!sidebar->isVisibleTo(f.window.get()));
        action("showSidebar")->trigger();
        QVERIFY(sidebar->isVisibleTo(f.window.get()));

        action("showSidebar")->trigger();
        action("focusSidebar")->trigger();
        QVERIFY(sidebar->isVisibleTo(f.window.get()));
        auto *tree = sidebar->findChild<QTreeView *>();
        QCOMPARE(f.window->focusWidget(), tree);
        QVERIFY(tree->currentIndex().isValid());
    }

    // A middle click closes a tab. Its root dashboard is kept.
    // A workspace is a data folder. Opening one loads it and remembers it on
    // this device, first in the recent list. A missing folder is reported and
    // dropped from the list.
    void workspaces()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(copyFixtures(dir.path()));

        Fixture f;
        f.window->openSampleData();
        QVERIFY(f.window->workspace().isEmpty());
        QVERIFY(f.window->windowTitle().contains(QStringLiteral("Sample data")));
        // The sample data is compiled into the app only, so the test checks no tabs here.

        const QString folder = QDir(dir.path()).absolutePath();
        f.window->openWorkspace(folder);
        QCOMPARE(f.window->workspace(), folder);
        QCOMPARE(f.window->recentWorkspaces(), QStringList{folder});
        QCOMPARE(f.settings.lastWorkspace(), folder);
        QCOMPARE(f.dataStore.dir(), folder);
        // The fixtures have a workspace.mirante naming them, which the title shows.
        QVERIFY(f.window->windowTitle().startsWith(QStringLiteral("Sample data")));
        QCOMPARE(f.bar()->count(), 2);
        QVERIFY(f.warningBar()->isHidden());

        // Opening it again keeps one entry; a missing folder is reported.
        f.window->openWorkspace(folder);
        QCOMPARE(f.window->recentWorkspaces(), QStringList{folder});
        f.window->openWorkspace(folder + QStringLiteral("/no-such-folder"));
        QCOMPARE(f.window->workspace(), folder);
        QVERIFY(!f.window->findChild<QWidget *>(QStringLiteral("warningBar"))->isHidden());

        // The path of workspace.mirante opens its folder.
        f.window->openSampleData();
        f.window->openWorkspace(folder + QStringLiteral("/workspace.mirante"));
        QCOMPARE(f.window->workspace(), folder);
    }

    // A workspace with a file from a newer version of the app is not opened,
    // and nothing in it is written. What was open stays open.
    void workspaceWithANewerFileIsRefused()
    {
        QTemporaryDir open;
        QTemporaryDir refused;
        QVERIFY(open.isValid() && refused.isValid());
        QVERIFY(copyFixtures(open.path()));
        QVERIFY(copyFixtures(refused.path()));
        const QString newerPath = refused.filePath(QStringLiteral("groups/newer.json"));
        const QByteArray newer = R"({"schemaVersion":9999,"id":"newer","name":"From the future"})";
        QFile file(newerPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(newer);
        file.close();

        Fixture f;
        const QString folder = QDir(open.path()).absolutePath();
        f.window->openWorkspace(folder);
        f.window->openWorkspace(refused.path());

        QCOMPARE(f.window->workspace(), folder);
        QCOMPARE(f.dataStore.dir(), folder);
        QCOMPARE(f.window->recentWorkspaces(), QStringList{folder});
        QVERIFY(!f.warningBar()->isHidden());
        QVERIFY(f.warningText().contains(QStringLiteral("newer version")));
        QVERIFY(f.warningText().contains(QStringLiteral("groups/newer.json")));

        // At the start, with nothing open, nothing is loaded.
        Fixture start;
        start.window->openWorkspace(refused.path());
        QVERIFY(start.window->workspace().isEmpty());
        QVERIFY(start.dataStore.dir().isEmpty());
        QVERIFY(!start.warningBar()->isHidden());

        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), newer);
    }

    // Another device may give the workspace a new id, for example when it
    // opened a copy synced before this device wrote the id. The logins of the
    // id this device knew move to the new one. An id this device gives, as
    // after the id was removed from a copy, takes nothing.
    void loginsFollowANewWorkspaceId()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid() && copyFixtures(dir.path()));
        const QString file = dir.filePath(QStringLiteral("workspace.mirante"));
        const auto setId = [&file](const QString &id) {
            QFile in(file);
            if (!in.open(QIODevice::ReadOnly))
                return false;
            QJsonObject json = QJsonDocument::fromJson(in.readAll()).object();
            in.close();
            if (id.isEmpty())
                json.remove(QStringLiteral("id"));
            else
                json.insert(QStringLiteral("id"), id);
            QFile out(file);
            return out.open(QIODevice::WriteOnly) && out.write(QJsonDocument(json).toJson()) > 0;
        };

        Fixture f(true);
        const QDir root(f.profilesRoot);
        QVERIFY(setId(QStringLiteral("first-id")));
        f.window->openWorkspace(dir.path());
        QCOMPARE(f.profiles->workspace(), QStringLiteral("first-id"));
        QCOMPARE(f.settings.workspaceId(dir.path()), QStringLiteral("first-id"));
        QVERIFY(QDir().mkpath(root.filePath(QStringLiteral("first-id/p-default/cache"))));

        f.window->openSampleData();
        QVERIFY(setId(QStringLiteral("second-id")));
        f.window->openWorkspace(dir.path());
        QCOMPARE(f.profiles->workspace(), QStringLiteral("second-id"));
        QVERIFY(QDir(root.filePath(QStringLiteral("second-id/p-default/cache"))).exists());
        QVERIFY(!QDir(root.filePath(QStringLiteral("first-id"))).exists());
        QCOMPARE(f.settings.workspaceId(dir.path()), QStringLiteral("second-id"));

        f.window->openSampleData();
        QVERIFY(setId(QString()));
        f.window->openWorkspace(dir.path());
        const QString given = f.model.workspaceId();
        QVERIFY(!given.isEmpty() && given != QStringLiteral("second-id"));
        QVERIFY(QDir(root.filePath(QStringLiteral("second-id/p-default/cache"))).exists());
        QCOMPARE(f.settings.workspaceId(dir.path()), given);
    }

    // A folder is a workspace only with a workspace.mirante in it. Without
    // one it is not opened; createWorkspace writes the file and opens it.
    void workspaceNeedsItsFile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        Fixture f;
        f.window->openSampleData();
        QVERIFY(!store::isWorkspace(dir.path()));
        f.window->openWorkspace(dir.path());
        QVERIFY(f.window->workspace().isEmpty());
        QVERIFY(!f.window->findChild<QWidget *>(QStringLiteral("warningBar"))->isHidden());

        QVERIFY(f.window->createWorkspace(dir.path(), QStringLiteral("Fresh")));
        QVERIFY(store::isWorkspace(dir.path()));
        QCOMPARE(f.window->workspace(), QDir(dir.path()).absolutePath());
        QVERIFY(f.window->windowTitle().startsWith(QStringLiteral("Fresh")));
        QVERIFY(f.window->findChild<QWidget *>(QStringLiteral("warningBar"))->isHidden());

        // A new workspace starts with one empty root dashboard, "Main", open
        // as the current tab and ready to fill.
        QCOMPARE(int(f.model.rootDashboards().size()), 1);
        const RootDashboard *main = f.model.rootDashboards().front();
        QCOMPARE(main->name, QStringLiteral("Main"));
        QCOMPARE(f.bar()->count(), 1);
        QCOMPARE(f.tabText(0), QStringLiteral("Main"));
        QVERIFY(f.bar()->selectionShown());
        QVERIFY(f.window->currentLayoutView());
        QVERIFY(f.window->currentLayoutView()->editMode());
        // It is written to disk like any other root dashboard.
        const QString mainId = main->id;
        f.dataStore.flush();
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("dashboards/%1.json").arg(mainId))));

        // It is ordinary: deleting it leaves the workspace without one, and
        // opening the workspace again does not add another.
        f.model.removeRootDashboard(mainId);
        f.dataStore.flush();
        f.window->openWorkspace(dir.path());
        QVERIFY(f.model.rootDashboards().empty());
        QCOMPARE(f.bar()->count(), 0);

        // A folder that does not exist cannot be made one.
        QVERIFY(!f.window->createWorkspace(dir.filePath(QStringLiteral("missing")), QStringLiteral("X")));
    }

    // A workspace's name lives in its workspace.mirante. Renaming writes it, and
    // the sidebar title and the window title show it.
    void renameWorkspace()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(copyFixtures(dir.path()));

        Fixture f;
        f.window->openWorkspace(dir.path());
        auto *title = f.window->findChild<QLabel *>(QStringLiteral("workspaceTitle"));
        QVERIFY(title);
        QCOMPARE(title->text(), QStringLiteral("Sample data"));

        QVERIFY(f.window->renameWorkspace(QStringLiteral("Work stuff")));
        QCOMPARE(title->text(), QStringLiteral("Work stuff"));
        QVERIFY(f.window->windowTitle().startsWith(QStringLiteral("Work stuff")));

        // The Store writes the name into workspace.mirante.
        f.dataStore.flush();
        QFile file(dir.filePath(QStringLiteral("workspace.mirante")));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("name")).toString(),
                 QStringLiteral("Work stuff"));
        file.close();

        // Opening it again reads the name back.
        f.window->openSampleData();
        f.window->openWorkspace(dir.path());
        QCOMPARE(title->text(), QStringLiteral("Work stuff"));

        // The sample data cannot be renamed.
        f.window->openSampleData();
        QVERIFY(!f.window->renameWorkspace(QStringLiteral("Other")));
    }

    // Dragging a root tab moves it, and its page goes with it. The order is
    // saved with the open tabs and comes back. The slots stay as they are.
    void reorderTabs()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString folder = QDir(root.path()).absoluteFilePath(QStringLiteral("workspace"));
        QVERIFY(copyFixtures(folder));

        Fixture f;
        f.window->openWorkspace(folder);
        f.window->resize(1000, 600);
        f.window->show();
        QVERIFY(QTest::qWaitForWindowExposed(f.window.get()));
        RootTabBar *bar = f.bar();
        QVERIFY(bar->isMovable());
        QVERIFY(!f.slotBar()->isMovable());
        QCOMPARE(bar->count(), 2);
        const QStringList before{f.tabText(0), f.tabText(1)};
        const QStringList after{before[1], before[0]};
        QMap<QString, LayoutView *> views;
        for (int i = 0; i < bar->count(); ++i) {
            emit bar->tabPressed(i);
            views[f.tabText(i)] = f.window->currentLayoutView();
        }
        const auto pagesInStep = [&] {
            for (int i = 0; i < bar->count(); ++i) {
                emit bar->tabPressed(i);
                if (f.window->currentLayoutView() != views.value(f.tabText(i)))
                    return false;
            }
            return true;
        };

        // A drag along the bar. Each mouse event goes to the bar with the
        // button held, as a real drag does.
        const auto moveTo = [bar](const QPoint &at) {
            QMouseEvent move(QEvent::MouseMove, QPointF(at), bar->mapToGlobal(QPointF(at)), Qt::NoButton,
                             Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(bar, &move);
        };
        QPoint at = bar->tabRect(0).center();
        const int end = bar->tabRect(1).right();
        QTest::mousePress(bar, Qt::LeftButton, {}, at);
        LayoutView *dragged = f.window->currentLayoutView();
        QCOMPARE(dragged, views.value(before[0]));
        while (at.x() < end) {
            at.rx() += 8;
            moveTo(at);
        }
        // Painting the bar places the image of the dragged tab under the mouse.
        // That image is QTabBar's only plain QWidget child.
        QVERIFY(!bar->grab().isNull());
        QWidget *image = nullptr;
        for (QWidget *child : bar->findChildren<QWidget *>(Qt::FindDirectChildrenOnly)) {
            if (child->isVisible() && QLatin1String(child->metaObject()->className()) == QLatin1String("QWidget"))
                image = child;
        }
        QVERIFY(image);
        QVERIFY(image->geometry().left() > bar->tabRect(0).center().x());
        QTest::mouseRelease(bar, Qt::LeftButton, {}, at);
        QCOMPARE(QStringList({f.tabText(0), f.tabText(1)}), after);
        QCOMPARE(bar->currentIndex(), 1);
        QCOMPARE(f.window->currentLayoutView(), dragged);
        QVERIFY(pagesInStep());

        // A drag that leaves the bar only moves the tab, as far as it gets.
        at = bar->tabRect(0).center();
        QTest::mousePress(bar, Qt::LeftButton, {}, at);
        moveTo(at + QPoint(0, 200));
        QTest::mouseRelease(bar, Qt::LeftButton, {}, at + QPoint(0, 200));
        QCOMPARE(bar->count(), 2);
        QCOMPARE(QStringList({f.tabText(0), f.tabText(1)}), after);
        QVERIFY(detachedWindows().empty());

        // A move made while a slot is current leaves the slot current.
        f.window->openFocus();
        bar->moveTab(0, 1);
        QCOMPARE(f.slotBar()->currentSlot(), int(SlotTabBar::FocusSlot));
        QVERIFY(!bar->selectionShown());
        QVERIFY(!bar->grab().isNull());
        bar->moveTab(0, 1);
        QVERIFY(pagesInStep());

        // The order is kept with the open tabs, and comes back.
        f.window->saveOpenTabs();
        QCOMPARE(int(f.settings.openTabs(folder).ids.size()), 2);
        QCOMPARE(f.model.rootDashboard(f.settings.openTabs(folder).ids[0])->name, after[0]);
        f.window->openSampleData();
        f.window->openWorkspace(folder);
        QCOMPARE(QStringList({f.tabText(0), f.tabText(1)}), after);

        // Detaching takes the tab that was clicked, in the new order.
        const QString firstId = f.settings.openTabs(folder).ids[0];
        emit bar->detachRequested(0);
        QCOMPARE(bar->count(), 1);
        QCOMPARE(f.tabText(0), before[0]);
        const auto windows = detachedWindows();
        QCOMPARE(windows.size(), 1);
        QCOMPARE(windows.front()->dashboardId(), firstId);
    }

    // Moving a workspace opens it in its new folder, keeps its open tabs and
    // its place in the recent list, and keeps writing there. Logins are kept
    // by profile id, so nothing about them changes.
    void moveWorkspace()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString from = QDir(root.path()).absoluteFilePath(QStringLiteral("old"));
        QVERIFY(copyFixtures(from));

        Fixture f;
        f.window->openWorkspace(from);
        f.window->openRootDashboard(kRelease);
        f.window->saveOpenTabs();
        const DeviceSettings::Tabs saved = f.settings.openTabs(from);

        const QString to = QDir(root.path()).absoluteFilePath(QStringLiteral("moved/here"));
        QVERIFY(f.window->moveWorkspace(to));
        QCOMPARE(f.window->workspace(), to);
        QVERIFY(!QFileInfo::exists(from));
        QVERIFY(store::isWorkspace(to));
        QCOMPARE(f.window->recentWorkspaces(), QStringList{to});
        QCOMPARE(f.settings.lastWorkspace(), to);
        QVERIFY(!f.settings.openTabs(from).saved);
        QCOMPARE(f.settings.openTabs(to).ids, saved.ids);
        QCOMPARE(f.bar()->count(), int(saved.ids.size()));
        QCOMPARE(f.settings.openTabs(to).current, saved.current);
        QVERIFY(f.window->findChild<QWidget *>(QStringLiteral("warningBar"))->isHidden());

        // Edits are written to the new folder.
        QVERIFY(f.window->renameWorkspace(QStringLiteral("Moved")));
        f.dataStore.flush();
        QFile file(QDir(to).filePath(QStringLiteral("workspace.mirante")));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("name")).toString(),
                 QStringLiteral("Moved"));
        file.close();

        // A folder that is not empty is refused; the workspace stays open
        // where it is, and the warning bar says why.
        QVERIFY(!f.window->moveWorkspace(root.path()));
        QCOMPARE(f.window->workspace(), to);
        QVERIFY(store::isWorkspace(to));
        QVERIFY(!f.window->findChild<QWidget *>(QStringLiteral("warningBar"))->isHidden());

        // The sample data cannot be moved.
        f.window->openSampleData();
        QVERIFY(!f.window->moveWorkspace(QDir(root.path()).absoluteFilePath(QStringLiteral("sample"))));
    }

    // An edit through EntityActions reaches the file on disk.
    void editReachesDisk()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(copyFixtures(dir.path()));

        Fixture f;
        f.window->openWorkspace(dir.path());
        QVERIFY(!f.dataStore.readOnly());
        QCOMPARE(f.model.task(kFixLoginTask)->state, TaskState::Active);
        f.window->entityActions()->setState(kFixLoginTask, TaskState::Done);
        f.dataStore.flush();

        QFile file(dir.filePath(QStringLiteral("tasks/%1.json").arg(kFixLoginTask)));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("state")).toString(),
                 QStringLiteral("done"));
    }

    // The open tabs of a workspace are saved when another one opens, and come
    // back when it opens again: root dashboards in order, and the current tab,
    // or Focus if its slot was current. The preview is not kept.
    void openTabsRestored()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(copyFixtures(dir.path()));

        Fixture f;
        f.window->openWorkspace(dir.path());
        QCOMPARE(f.bar()->count(), 2);      // nothing saved yet: every root dashboard
        f.window->openRootDashboard(kRelease);
        f.window->closeCurrentTab();
        f.window->openTaskPreview(kFixLoginTask);
        QCOMPARE(f.bar()->count(), 1);

        // The preview was current, so the first tab is current now.
        f.window->openSampleData();
        f.window->openWorkspace(dir.path());
        QCOMPARE(f.bar()->count(), 1);
        QCOMPARE(f.tabText(0), QStringLiteral("Morning"));
        QVERIFY(!f.slotBar()->hasPreview());
        QCOMPARE(f.slotBar()->currentSlot(), -1);
        QCOMPARE(f.bar()->currentIndex(), 0);

        // Reloading keeps them, and so does a new window on the same settings.
        f.window->openRootDashboard(kRelease);
        f.window->reloadWorkspace();
        QCOMPARE(f.bar()->count(), 2);
        QCOMPARE(f.bar()->currentIndex(), 1);
        f.window->openFocus();
        f.window->close();
        f.window.reset();
        f.dataStore.close();    // as at start: nothing is open yet
        f.window = std::make_unique<MainWindow>(f.model, f.types, f.dataStore, f.settings);
        f.window->openWorkspace(dir.path());
        QCOMPARE(f.bar()->count(), 2);
        QCOMPARE(f.tabText(0), QStringLiteral("Morning"));
        // Focus was current, so it is current again; Focus is not a saved tab.
        QCOMPARE(f.slotBar()->currentSlot(), int(SlotTabBar::FocusSlot));
        const DeviceSettings::Tabs saved = f.settings.openTabs(f.dataStore.dir());
        QCOMPARE(saved.ids, (QStringList{kMorning, kRelease}));
        QCOMPARE(saved.current, QStringLiteral("focus"));
    }

    // A "focus" id saved as a tab by earlier versions is skipped, and still
    // makes the Focus slot current.
    void oldFocusTabRestored()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(copyFixtures(dir.path()));

        Fixture f;
        DeviceSettings::Tabs tabs;
        tabs.saved = true;
        tabs.ids = QStringList{QStringLiteral("focus"), kRelease};
        tabs.current = kRelease;
        f.settings.setOpenTabs(QDir(dir.path()).absolutePath(), tabs);
        f.window->openWorkspace(dir.path());
        QCOMPARE(f.bar()->count(), 1);
        QCOMPARE(f.tabText(0), QStringLiteral("Release"));
        QCOMPARE(f.slotBar()->currentSlot(), -1);
    }

    // The Focus entry selects the Focus slot, which is not a tab.
    // A click on the Focus slot, or openFocus(), makes Focus current. The
    // sidebar has no Focus entry (decision 0053).
    void focusSlotSelectsFocus()
    {
        Fixture f;
        f.window->show();
        QVERIFY(QTest::qWaitForWindowExposed(f.window.get()));
        QVERIFY(!f.window->findChild<Sidebar *>()->findChild<QPushButton *>(QStringLiteral("focusEntry")));
        QTest::mouseClick(f.slotBar(), Qt::LeftButton, {}, f.slotBar()->tabRect(SlotTabBar::FocusSlot).center());
        QCOMPARE(f.bar()->count(), 2);
        QCOMPARE(f.slotBar()->currentSlot(), int(SlotTabBar::FocusSlot));
        QVERIFY(!f.bar()->selectionShown());
        QVERIFY(!f.window->currentLayoutView());

        f.window->openRootDashboard(kRelease);
        QCOMPARE(f.slotBar()->currentSlot(), -1);
        f.window->openFocus();
        QCOMPARE(f.slotBar()->currentSlot(), int(SlotTabBar::FocusSlot));
        QCOMPARE(f.window->findChildren<FocusView *>().size(), 1);

        // A task opened from it goes to the preview slot.
        auto *focus = f.window->findChild<FocusView *>();
        QVERIFY(focus);
        emit focus->taskOpened(kFixLoginTask);
        QCOMPARE(f.bar()->count(), 2);
        QCOMPARE(f.slotBar()->currentSlot(), int(SlotTabBar::PreviewSlot));
        QCOMPARE(f.previewText(), QStringLiteral("Fix login timeout / Work"));

        // Closing the last tab shows Focus.
        f.window->openRootDashboard(kRelease);
        emit f.bar()->tabCloseRequested(1);
        emit f.bar()->tabCloseRequested(0);
        QCOMPARE(f.bar()->count(), 0);
        QCOMPARE(f.slotBar()->currentSlot(), int(SlotTabBar::FocusSlot));
    }

    // The sample data is read-only, and the warning bar says so.
    void readOnlyNote()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(copyFixtures(dir.path()));

        Fixture f;
        QVERIFY(f.warningBar()->isHidden());
        f.window->openSampleData();
        QVERIFY(f.dataStore.readOnly());
        QVERIFY(!f.warningBar()->isHidden());
        QVERIFY(f.warningText().contains(QStringLiteral("not saved")));

        f.window->openWorkspace(dir.path());
        QVERIFY(f.warningBar()->isHidden());
    }

    // The + menu in the sidebar header makes a project in the selected group
    // and a task in the selected project, so each needs a selection.
    void newMenu()
    {
        Fixture f;
        auto *button = f.window->findChild<QToolButton *>(QStringLiteral("newEntityButton"));
        QVERIFY(button && button->menu());
        const auto actions = button->menu()->actions();
        QCOMPARE(actions.size(), 3);
        emit button->menu()->aboutToShow();
        QVERIFY(actions[0]->isEnabled());
        QVERIFY(!actions[1]->isEnabled());
        QVERIFY(!actions[2]->isEnabled());

        f.window->revealInSidebar(kFixLoginTask);
        emit button->menu()->aboutToShow();
        QVERIFY(actions[1]->isEnabled());
        QVERIFY(actions[2]->isEnabled());
    }

    // A pasted image that cannot be placed is explained in the status bar.
    // The registry has no type that takes pasted images.
    // No paste dialog opens for a paste that cannot happen.
    void pasteImageMessage()
    {
        Fixture f;
        int dialogs = 0;
        f.window->setExec([&dialogs](QDialog &) {
            ++dialogs;
            return true;
        });
        QImage image(4, 4, QImage::Format_ARGB32);
        image.fill(Qt::red);
        f.window->pasteImage(image);
        QCOMPARE(f.window->statusBar()->currentMessage(), ImagePaste::message(ImagePaste::Result::NoImageType));
        QCOMPARE(dialogs, 0);
    }

    // A paste into an item alone in a tab opens the paste dialog first.
    // Cancel pastes nothing; Paste saves the cropped image, at full size.
    void pasteImageIntoItemCrops()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(copyFixtures(dir.path()));
        Fixture f;
        f.types.registerModule(QStringLiteral("image"), &registerTestImage);
        f.window->openWorkspace(dir.path());
        f.window->openItem(kImageTask, kGardenSketch);
        const auto file = [&f] { return f.model.item(kImageTask, kGardenSketch)->config.value(QStringLiteral("file")).toString(); };
        const QString before = file();

        QImage image(300, 200, QImage::Format_RGB32);
        image.fill(Qt::blue);
        QPainter(&image).fillRect(QRect(100, 50, 40, 30), Qt::green);

        QStringList dialogs;
        f.window->setExec([&dialogs](QDialog &dialog) {
            dialogs << QString::fromLatin1(dialog.metaObject()->className());
            return false;
        });
        f.window->pasteImage(image);
        QCOMPARE(dialogs, QStringList{QStringLiteral("mirante::ImagePasteDialog")});
        QCOMPARE(file(), before);

        f.window->setExec(cropTo(QRect(100, 50, 40, 30)));
        f.window->pasteImage(image);
        QVERIFY(file() != before);
        const QImage saved(f.dataStore.filePath(file()));
        QCOMPARE(saved.size(), QSize(40, 30));
        QCOMPARE(saved.pixelColor(0, 0), QColor(Qt::green));
        QCOMPARE(saved.pixelColor(39, 29), QColor(Qt::green));
    }

    // Ctrl+V never adds an image to a dashboard. On an unlocked root or task
    // dashboard without a focused image item, the status bar points to the
    // Items menu, no dialog opens, and nothing changes.
    void pasteImageNeverAddsToDashboard()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(copyFixtures(dir.path()));
        Fixture f;
        f.types.registerModule(QStringLiteral("image"), &registerTestImage);
        f.window->openWorkspace(dir.path());
        int dialogs = 0;
        f.window->setExec([&dialogs](QDialog &) {
            ++dialogs;
            return true;
        });
        const Task taskBefore = *f.model.task(kImageTask);
        const RootDashboard rootBefore = *f.model.rootDashboard(kMorning);
        QImage image(30, 20, QImage::Format_RGB32);
        image.fill(Qt::red);

        f.window->openRootDashboard(kMorning);
        QToolButton *rootLock = nullptr;
        for (QToolButton *button : f.window->findChildren<QToolButton *>(QStringLiteral("lockButton")))
            rootLock = button;
        QVERIFY(rootLock);
        rootLock->click();
        QVERIFY(f.window->currentLayoutView()->editMode());
        f.window->pasteImage(image);
        QCOMPARE(f.window->statusBar()->currentMessage(), ImagePaste::message(ImagePaste::Result::NoImageItem));

        f.window->openTaskPreview(kImageTask);
        f.window->pasteImage(image);
        QCOMPARE(f.window->statusBar()->currentMessage(), ImagePaste::message(ImagePaste::Result::NoImageItem));

        QCOMPARE(dialogs, 0);
        QVERIFY(*f.model.task(kImageTask) == taskBefore);
        QVERIFY(*f.model.rootDashboard(kMorning) == rootBefore);
    }

    // "Paste Image…" in a task's Items menu opens the paste dialog, then adds
    // the cropped image to the task as a new Image item. No dashboard
    // changes. Cancel adds nothing.
    void pasteImageFromItemsMenu()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(copyFixtures(dir.path()));
        Fixture f;
        f.types.registerModule(QStringLiteral("image"), &registerTestImage);
        f.window->openWorkspace(dir.path());
        EntityActions *actions = f.window->entityActions();
        QImage image(300, 200, QImage::Format_RGB32);
        image.fill(Qt::blue);
        QPainter(&image).fillRect(QRect(100, 50, 40, 30), Qt::green);
        actions->setClipboardImage([&image] { return image; });

        const Task before = *f.model.task(kImageTask);
        const auto roots = [&f] {
            std::vector<RootDashboard> values;
            for (const RootDashboard *root : f.model.rootDashboards())
                values.push_back(*root);
            return values;
        };
        const std::vector<RootDashboard> rootsBefore = roots();
        const auto trigger = [actions] {
            QMenu menu;
            actions->fillMenu(&menu, SidebarModel::Kind::ItemList, QString(), kImageTask);
            QAction *paste = nullptr;
            for (QAction *action : menu.actions()) {
                if (action->text() == QStringLiteral("Paste Image…"))
                    paste = action;
            }
            QVERIFY(paste && paste->isEnabled());
            paste->trigger();
        };

        QStringList dialogs;
        actions->setExec([&dialogs](QDialog &dialog) {
            dialogs << QString::fromLatin1(dialog.metaObject()->className());
            return false;
        });
        trigger();
        QCOMPARE(dialogs, QStringList{QStringLiteral("mirante::ImagePasteDialog")});
        QVERIFY(*f.model.task(kImageTask) == before);

        actions->setExec(cropTo(QRect(100, 50, 40, 30)));
        trigger();
        const Task *task = f.model.task(kImageTask);
        QCOMPARE(task->items.size(), before.items.size() + 1);
        const Item &item = task->items.back();
        QCOMPARE(item.type, QStringLiteral("image"));
        const QImage saved(f.dataStore.filePath(item.config.value(QStringLiteral("file")).toString()));
        QCOMPARE(saved.size(), QSize(40, 30));
        QCOMPARE(saved.pixelColor(0, 0), QColor(Qt::green));
        // The item is on no dashboard: the task's dashboards and the root
        // dashboards are unchanged.
        QVERIFY(task->dashboards == before.dashboards);
        QVERIFY(roots() == rootsBefore);
    }

    // In read-only data, such as the sample data, "Paste Image…" says why in
    // the status bar and opens no dialog.
    void pasteImageFromItemsMenuReadOnly()
    {
        Fixture f;
        f.types.registerModule(QStringLiteral("image"), &registerTestImage);
        QVERIFY(f.dataStore.readOnly());
        EntityActions *actions = f.window->entityActions();
        QImage image(30, 20, QImage::Format_RGB32);
        image.fill(Qt::red);
        actions->setClipboardImage([&image] { return image; });
        int dialogs = 0;
        actions->setExec([&dialogs](QDialog &) {
            ++dialogs;
            return true;
        });
        const size_t itemsBefore = f.model.task(kImageTask)->items.size();
        QVERIFY(actions->pasteImage(kImageTask).isEmpty());
        QCOMPARE(dialogs, 0);
        QCOMPARE(f.window->statusBar()->currentMessage(), ImagePaste::message(ImagePaste::Result::SaveFailed));
        QCOMPARE(f.model.task(kImageTask)->items.size(), itemsBefore);
    }

    // A deleted task's preview closes.
    void removedTaskClosesItsTabs()
    {
        Fixture f;
        f.window->openTaskPreview(kFixLoginTask, QStringLiteral("ece7e850-6e0f-51ef-ad23-4ee948afdd66"));
        QCOMPARE(f.bar()->count(), 2);
        QVERIFY(f.slotBar()->hasPreview());
        f.model.removeTask(kFixLoginTask);
        QCOMPARE(f.bar()->count(), 2);
        QVERIFY(!f.slotBar()->hasPreview());
        QCOMPARE(f.slotBar()->currentSlot(), -1);
    }

    // A task moved to the trash, alone or with its project, closes its
    // preview and does not open again until restored.
    void trashedTaskClosesItsTabs()
    {
        Fixture f;
        f.window->openTaskPreview(kFixLoginTask);
        QVERIFY(f.slotBar()->hasPreview());
        Task task = *f.model.task(kFixLoginTask);
        task.trashedAt = QStringLiteral("2026-10-29T10:00:00Z");
        f.model.setTask(task);
        QVERIFY(!f.slotBar()->hasPreview());
        f.window->openTaskPreview(kFixLoginTask);
        QVERIFY(!f.slotBar()->hasPreview());

        task.trashedAt.clear();
        f.model.setTask(task);
        f.window->openTaskPreview(kFixLoginTask);
        QVERIFY(f.slotBar()->hasPreview());
        Project project = *f.model.project(task.projectId);
        project.trashedAt = QStringLiteral("2026-10-29T10:00:00Z");
        f.model.setProject(project);
        QVERIFY(!f.slotBar()->hasPreview());
        QCOMPARE(f.bar()->count(), 2);
    }

    // The sidebar tints the row of what the hovered cell shows: the task of a
    // task cell, or the item of an item cell, also inside a task cell.
    void hoverTintsSidebarRow()
    {
        Fixture f;
        auto *sidebar = f.window->findChild<Sidebar *>();
        auto *delegate = qobject_cast<SidebarDelegate *>(sidebar->findChild<QTreeView *>()->itemDelegate());
        QVERIFY(delegate);
        TaskCell *taskCell = nullptr;
        for (TaskCell *cell : f.window->currentLayoutView()->findChildren<TaskCell *>()) {
            if (cell->taskId() == kFixLoginTask)
                taskCell = cell;
        }
        QVERIFY(taskCell);
        CellFrame *itemCell = taskCell->view()->findChild<CellFrame *>();
        QVERIFY(itemCell && itemCell->ref());
        const QString itemId = itemCell->ref()->item;

        QEnterEvent enter(QPointF(1, 1), QPointF(1, 1), QPointF(1, 1));
        QEvent leave(QEvent::Leave);
        QCoreApplication::sendEvent(taskCell, &enter);
        QCOMPARE(sidebar->hoverHighlight(), kFixLoginTask);
        QVERIFY(delegate->isHoverTinted(sidebar->model()->indexOf(kFixLoginTask)));

        // The task is collapsed, so its row stands in for the hidden item row.
        QCoreApplication::sendEvent(itemCell, &enter);
        QCOMPARE(sidebar->hoverHighlight(), itemId);
        QCOMPARE(sidebar->hoverRow(), kFixLoginTask);
        QVERIFY(delegate->isHoverTinted(sidebar->model()->indexOf(kFixLoginTask)));

        // Once the item row can be seen, the tint moves to it. With the task
        // open but its Items row closed, the task stands in again.
        auto *tree = sidebar->findChild<QTreeView *>();
        const QModelIndex itemIndex = sidebar->model()->indexOf(itemId);
        tree->expand(itemIndex.parent().parent());
        tree->expand(itemIndex.parent());
        QCOMPARE(sidebar->hoverRow(), itemId);
        QVERIFY(delegate->isHoverTinted(itemIndex));
        QVERIFY(!delegate->isHoverTinted(sidebar->model()->indexOf(kFixLoginTask)));
        tree->collapse(itemIndex.parent());
        QCOMPARE(sidebar->hoverRow(), kFixLoginTask);
        tree->expand(itemIndex.parent());

        // A collapsed project stands in for everything under it.
        tree->collapse(sidebar->model()->indexOf(f.model.task(kFixLoginTask)->projectId));
        QCOMPARE(sidebar->hoverRow(), f.model.task(kFixLoginTask)->projectId);

        QCoreApplication::sendEvent(itemCell, &leave);
        QCOMPARE(sidebar->hoverHighlight(), kFixLoginTask);
        QCoreApplication::sendEvent(taskCell, &leave);
        QVERIFY(sidebar->hoverHighlight().isEmpty());
    }

    // The settings window turns the sidebar's hover tint off and on at once.
    // Its action has the preferences role, so macOS shows it in the
    // application menu; elsewhere it is in the Tools menu.
    void settingsTurnHoverTintOff()
    {
        Fixture f;
        auto *action = f.window->findChild<QAction *>(QStringLiteral("settingsAction"));
        QVERIFY(action);
        QCOMPARE(action->menuRole(), QAction::PreferencesRole);
        QCOMPARE(action->text(), QStringLiteral("&Settings…"));
        auto *tools = f.window->menuBar()->findChild<QMenu *>(QStringLiteral("toolsMenu"));
#ifdef Q_OS_MACOS
        QVERIFY(!tools);
#else
        QVERIFY(tools && tools->actions().contains(action));
#endif

        auto *sidebar = f.window->findChild<Sidebar *>();
        TaskCell *taskCell = nullptr;
        for (TaskCell *cell : f.window->currentLayoutView()->findChildren<TaskCell *>()) {
            if (cell->taskId() == kFixLoginTask)
                taskCell = cell;
        }
        QVERIFY(taskCell);
        QEnterEvent enter(QPointF(1, 1), QPointF(1, 1), QPointF(1, 1));
        QCoreApplication::sendEvent(taskCell, &enter);
        QCOMPARE(sidebar->hoverHighlight(), kFixLoginTask);

        action->trigger();
        auto *dialog = f.window->findChild<SettingsDialog *>();
        QVERIFY(dialog);
        QVERIFY(dialog->sidebarHoverTint()->isChecked());

        // Off: the tint goes at once, and the mouse brings none.
        dialog->sidebarHoverTint()->setChecked(false);
        QVERIFY(!f.settings.sidebarHoverTint());
        QVERIFY(sidebar->hoverHighlight().isEmpty());
        QEvent leave(QEvent::Leave);
        QCoreApplication::sendEvent(taskCell, &leave);
        QCoreApplication::sendEvent(taskCell, &enter);
        QVERIFY(sidebar->hoverHighlight().isEmpty());

        // On: the hovered cell's row is tinted again at once.
        dialog->sidebarHoverTint()->setChecked(true);
        QCOMPARE(sidebar->hoverHighlight(), kFixLoginTask);

        // A window started with the setting off does not tint.
        dialog->sidebarHoverTint()->setChecked(false);
        QCoreApplication::sendEvent(taskCell, &leave);
        f.window = std::make_unique<MainWindow>(f.model, f.types, f.dataStore, f.settings);
        sidebar = f.window->findChild<Sidebar *>();
        for (TaskCell *cell : f.window->currentLayoutView()->findChildren<TaskCell *>()) {
            if (cell->taskId() == kFixLoginTask)
                taskCell = cell;
        }
        QCoreApplication::sendEvent(taskCell, &enter);
        QVERIFY(sidebar->hoverHighlight().isEmpty());
    }

    // From the sidebar menu, a task dashboard or a single item opens in a new
    // root dashboard.
    void openInNewRootDashboard()
    {
        Fixture f;
        auto *sidebar = f.window->findChild<Sidebar *>();
        emit sidebar->openInNewRootDashboard(kFixLoginTask, kFixLoginDashboard);
        QCOMPARE(int(f.model.rootDashboards().size()), 3);
        const LayoutNode &taskTree = f.window->currentLayoutView()->tree();
        QVERIFY(taskTree.ref && taskTree.ref->isTask());
        QCOMPARE(taskTree.ref->dashboard, kFixLoginDashboard);

        emit sidebar->openItemInNewRootDashboard(kOverviewTask, kReviewsItem);
        QCOMPARE(int(f.model.rootDashboards().size()), 4);
        const LayoutNode &itemTree = f.window->currentLayoutView()->tree();
        QVERIFY(itemTree.ref && itemTree.ref->isItem());
        QCOMPARE(itemTree.ref->item, kReviewsItem);
        QCOMPARE(itemTree.ref->task, kOverviewTask);
    }

    void middleClickClosesTab()
    {
        Fixture f;
        f.window->show();
        QVERIFY(QTest::qWaitForWindowExposed(f.window.get()));
        QTRY_VERIFY(f.bar()->height() > 0);
        QTest::mouseClick(f.bar(), Qt::MiddleButton, {}, f.bar()->tabRect(0).center());
        QCOMPARE(f.bar()->count(), 1);
        QCOMPARE(f.tabText(0), QStringLiteral("Release"));
        QVERIFY(f.model.rootDashboard(kMorning));
    }

    // The list holds every root dashboard. It opens closed ones again, and is
    // the only place where one is deleted.
    void dashboardList()
    {
        Fixture f;
        f.window->closeCurrentTab();    // closes Morning
        QCOMPARE(f.bar()->count(), 1);

        f.window->showDashboardList();
        auto *list = f.window->findChild<RootDashboardsDialog *>();
        QVERIFY(list);
        QCOMPARE(list->ids(), (QStringList{kMorning, kRelease}));

        list->select(kMorning);
        list->openSelected();
        QCOMPARE(f.bar()->count(), 2);
        QCOMPARE(f.tabText(f.bar()->currentIndex()), QStringLiteral("Morning"));

        // Deleting asks first. Saying no keeps the dashboard.
        QString asked;
        list->setConfirmDelete([&asked](const QString &name) {
            asked = name;
            return false;
        });
        list->deleteSelected();
        QCOMPARE(asked, QStringLiteral("Morning"));
        QVERIFY(f.model.rootDashboard(kMorning));

        list->setConfirmDelete([](const QString &) { return true; });
        list->deleteSelected();
        QVERIFY(!f.model.rootDashboard(kMorning));
        QCOMPARE(f.bar()->count(), 1);
        QCOMPARE(list->ids(), QStringList{kRelease});
    }

    // An item opens alone like a task dashboard: in the preview slot, with
    // its type icon and a "Task / Item" title bar. It never becomes a tab.
    void itemTab()
    {
        Fixture f;
        f.window->openItem(kOverviewTask, kReviewsItem);
        QCOMPARE(f.bar()->count(), 2);
        QCOMPARE(f.slotBar()->currentSlot(), int(SlotTabBar::PreviewSlot));
        QVERIFY(!f.slotBar()->tabIcon(SlotTabBar::PreviewSlot).isNull());
        const Item *item = f.model.item(kOverviewTask, kReviewsItem);
        const QString title = QStringLiteral("%1 / %2").arg(f.model.task(kOverviewTask)->title, item->name);
        QCOMPARE(f.previewText(), title);

        // The page shows only the item, read-only, with no lock.
        LayoutView *view = f.window->currentLayoutView();
        QCOMPARE(view->tree().kind, LayoutNode::Kind::Cell);
        QVERIFY(view->tree().ref);
        QCOMPARE(view->tree().ref->item, kReviewsItem);
        QVERIFY(!view->editMode());
        QVERIFY(!view->parentWidget()->findChild<QToolButton *>(QStringLiteral("taskLockButton")));
        const auto frames = view->findChildren<CellFrame *>();
        QCOMPARE(frames.size(), 1);

        // Opening it again switches to the preview. A task dashboard then
        // replaces it in the preview slot.
        f.window->openRootDashboard(kMorning);
        f.window->openItem(kOverviewTask, kReviewsItem);
        QCOMPARE(f.slotBar()->currentSlot(), int(SlotTabBar::PreviewSlot));
        QCOMPARE(f.window->currentLayoutView(), view);
        f.window->openTaskPreview(kFixLoginTask);
        QCOMPARE(f.bar()->count(), 2);
        QVERIFY(f.slotBar()->hasPreview());
        f.window->openItem(kOverviewTask, kReviewsItem);
        QCOMPARE(f.bar()->count(), 2);
        QCOMPARE(f.previewText(), title);

        // The tabs and the preview close when their items and tasks are gone.
        QVERIFY(store::load(QStringLiteral(MIRANTE_FIXTURES_DIR "/no-such-folder"), f.model).empty());
        QCOMPARE(f.bar()->count(), 0);
        QVERIFY(!f.slotBar()->hasPreview());
    }

    void focusItem()
    {
        Fixture f;
        f.window->show();
        f.window->activateWindow();
        if (!QTest::qWaitForWindowActive(f.window.get()))
            QSKIP("The window cannot be activated on this platform.");

        f.window->focusItem(kOverviewTask, kReviewsItem);
        QCOMPARE(f.slotBar()->currentSlot(), int(SlotTabBar::PreviewSlot));
        QWidget *focused = QApplication::focusWidget();
        QVERIFY(focused);
        auto *frame = qobject_cast<CellFrame *>(focused);
        QVERIFY(frame);
        QVERIFY(frame->ref());
        QCOMPARE(frame->ref()->item, kReviewsItem);
    }

    // Detaching a root dashboard tab moves its page into a window of its own.
    // The page keeps its LayoutView and its cells. The window's lock is the
    // dashboard's lock.
    void detachMovesPage()
    {
        Fixture f;
        LayoutView *view = f.window->currentLayoutView();
        QVERIFY(view);
        const std::vector<QWidget *> cells = view->visibleCells();
        QVERIFY(!cells.empty());
        QVERIFY(f.bar()->tabData(0).toBool());

        emit f.bar()->detachRequested(0);
        QCOMPARE(f.bar()->count(), 1);
        QCOMPARE(f.tabText(0), QStringLiteral("Release"));
        const auto windows = detachedWindows();
        QCOMPARE(windows.size(), 1);
        DetachedWindow *window = windows.front();
        QCOMPARE(window->dashboardId(), kMorning);
        QCOMPARE(window->windowTitle(), QStringLiteral("Morning"));
        QVERIFY(window->isVisible());
        QVERIFY(window->page());
        QCOMPARE(window->page()->view, view);
        QVERIFY(view->window() == window);
        QCOMPARE(view->visibleCells(), cells);

        // The window's lock unlocks the dashboard and turns on edit mode.
        auto *lock = window->findChild<QToolButton *>(QStringLiteral("lockButton"));
        QVERIFY(lock);
        QVERIFY(!view->editMode());
        lock->click();
        QVERIFY(view->editMode());
        QCOMPARE(lock->text(), QStringLiteral("Editing"));
        lock->click();
        QVERIFY(!view->editMode());

        // A rename changes its title.
        RootDashboard renamed = *f.model.rootDashboard(kMorning);
        renamed.name = QStringLiteral("Mornings");
        f.model.setRootDashboard(renamed);
        QCOMPARE(window->windowTitle(), QStringLiteral("Mornings"));

        // Only root dashboard tabs can be detached. Focus and the preview are
        // not tabs.
        f.window->openTaskPreview(kFixLoginTask);
        QCOMPARE(f.bar()->count(), 1);
        QVERIFY(f.bar()->tabData(0).toBool());
        QCOMPARE(detachedWindows().size(), 1);
    }

    // Closing the window, or "Put Back", returns the page to the end of the
    // tab bar and makes it current.
    void closingPutsBack()
    {
        Fixture f;
        LayoutView *view = f.window->currentLayoutView();
        emit f.bar()->detachRequested(0);
        QPointer<DetachedWindow> window = detachedWindows().value(0);
        QVERIFY(window);
        window->close();
        QCOMPARE(f.bar()->count(), 2);
        QCOMPARE(f.tabText(1), QStringLiteral("Morning"));
        QCOMPARE(f.bar()->currentIndex(), 1);
        QCOMPARE(f.window->currentLayoutView(), view);
        QVERIFY(view->window() == f.window.get());
        QTRY_VERIFY(!window);
        QVERIFY(!f.settings.detachedGeometry(kMorning).isEmpty());

        f.window->openTaskPreview(kFixLoginTask);
        f.window->openRootDashboard(kRelease);
        emit f.bar()->detachRequested(0);
        window = detachedWindows().value(0);
        QVERIFY(window);
        auto *putBack = window->findChild<QToolButton *>(QStringLiteral("putBackButton"));
        QVERIFY(putBack);
        putBack->click();
        QCOMPARE(f.bar()->count(), 2);
        QCOMPARE(f.tabText(1), QStringLiteral("Release"));
        QVERIFY(f.slotBar()->hasPreview());
        QCOMPARE(f.slotBar()->currentSlot(), -1);
        QCOMPARE(f.bar()->currentIndex(), 1);
        QTRY_VERIFY(!window);
    }

    // Opening a detached root dashboard, from the sidebar, the switcher or
    // the dashboard list, shows its window instead of a tab.
    void openingDetachedRaisesWindow()
    {
        Fixture f;
        emit f.bar()->detachRequested(0);
        QCOMPARE(f.bar()->count(), 1);
        DetachedWindow *window = detachedWindows().value(0);
        QVERIFY(window);
        window->hide();
        f.window->openRootDashboard(kMorning);
        QCOMPARE(f.bar()->count(), 1);
        QCOMPARE(detachedWindows().size(), 1);
        QVERIFY(window->isVisible());
    }

    // Deleting a root dashboard closes its window.
    void removingClosesWindow()
    {
        Fixture f;
        emit f.bar()->detachRequested(0);
        QPointer<DetachedWindow> window = detachedWindows().value(0);
        QVERIFY(window);
        f.model.removeRootDashboard(kMorning);
        QVERIFY(!window);
        QCOMPARE(f.bar()->count(), 1);
        QCOMPARE(detachedWindows().size(), 0);
    }

    // The detached root dashboards are saved with the open tabs, and come
    // back in their windows at their stored geometry.
    void detachedRestored()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(copyFixtures(dir.path()));

        Fixture f;
        f.window->openWorkspace(dir.path());
        emit f.bar()->detachRequested(0);
        DetachedWindow *window = detachedWindows().value(0);
        QVERIFY(window);
        window->resize(640, 480);
        f.window->saveOpenTabs();
        const DeviceSettings::Tabs tabs = f.settings.openTabs(QDir(dir.path()).absolutePath());
        QCOMPARE(tabs.detached, QStringList{kMorning});
        QCOMPARE(tabs.ids, QStringList{kRelease});

        // Another workspace closes the window, which keeps its geometry.
        f.window->openSampleData();
        QCOMPARE(detachedWindows().size(), 0);
        QVERIFY(!f.settings.detachedGeometry(kMorning).isEmpty());

        f.window->openWorkspace(dir.path());
        QCOMPARE(f.bar()->count(), 1);
        QCOMPARE(f.tabText(0), QStringLiteral("Release"));
        window = detachedWindows().value(0);
        QVERIFY(window);
        QCOMPARE(window->dashboardId(), kMorning);
        QCOMPARE(window->size(), QSize(640, 480));

        // Reloading keeps it detached.
        f.window->reloadWorkspace();
        QCOMPARE(f.bar()->count(), 1);
        QCOMPARE(detachedWindows().size(), 1);

        // Closing the main window puts it back, after saving it as detached,
        // so a new window opens it detached again.
        f.window->close();
        QCOMPARE(f.bar()->count(), 2);
        QCOMPARE(detachedWindows().size(), 0);
        f.window.reset();
        f.dataStore.close();
        f.window = std::make_unique<MainWindow>(f.model, f.types, f.dataStore, f.settings);
        f.window->openWorkspace(dir.path());
        QCOMPARE(f.bar()->count(), 1);
        QCOMPARE(detachedWindows().size(), 1);
        QCOMPARE(detachedWindows().front()->size(), QSize(640, 480));
    }

    // A cell in a detached window is maximised within its own window's page.
    void maximiseInDetachedWindow()
    {
        Fixture f;
        f.window->show();
        emit f.bar()->detachRequested(0);
        DetachedWindow *window = detachedWindows().value(0);
        QVERIFY(window);
        CellFrame *cell = itemCell(window->page());
        QVERIFY(cell);

        f.window->shortcuts()->toggleMaximised(cell);
        QVERIFY(f.window->shortcuts()->isMaximised(cell));
        QCOMPARE(cell->window(), window);
        QWidget *layer = cell->parentWidget();
        QCOMPARE(layer->objectName(), QStringLiteral("maximiseLayer"));
        QCOMPARE(layer->parentWidget(), window->page());
        QCOMPARE(layer->geometry(), window->page()->rect());

        // Changing the main window's tab leaves it maximised.
        f.window->openTaskPreview(kFixLoginTask);
        QVERIFY(f.window->shortcuts()->isMaximised(cell));

        f.window->shortcuts()->toggleMaximised(cell);
        QVERIFY(!f.window->shortcuts()->isMaximised(cell));
        QCOMPARE(cell->window(), window);
        QVERIFY(window->page()->view->isAncestorOf(cell));

        // A cell of the main window is maximised over its content area.
        CellFrame *mainCell = itemCell(f.window->currentLayoutView());
        QVERIFY(mainCell);
        f.window->shortcuts()->toggleMaximised(mainCell);
        QCOMPARE(mainCell->parentWidget()->parentWidget(), f.window->contentArea());
        f.window->shortcuts()->toggleMaximised(mainCell);
        QVERIFY(f.window->currentLayoutView()->isAncestorOf(mainCell));
    }

    // The cell's maximise button and menu reach Shortcuts, and "Open Alone"
    // reaches openItem.
    void biggerViewCallbacks()
    {
        Fixture f;
        f.window->show();
        QVERIFY(QTest::qWaitForWindowExposed(f.window.get()));
        LayoutView *view = f.window->currentLayoutView();
        CellFrame *cell = itemCell(view);
        QVERIFY(cell);
        const QString itemId = cell->ref()->item;
        const auto path = view->pathOf(cell);
        QVERIFY(path);

        emit cell->maximiseRequested();
        QVERIFY(f.window->shortcuts()->isMaximised(cell));
        chooseInCellMenu(*view, *path, QStringLiteral("Restore"));
        QTRY_VERIFY(!f.window->shortcuts()->isMaximised(cell));
        chooseInCellMenu(*view, *path, QStringLiteral("Maximise"));
        QTRY_VERIFY(f.window->shortcuts()->isMaximised(cell));
        emit cell->maximiseRequested();
        QVERIFY(!f.window->shortcuts()->isMaximised(cell));

        chooseInCellMenu(*view, *path, QStringLiteral("Open Alone"));
        QTRY_COMPARE(f.slotBar()->currentSlot(), int(SlotTabBar::PreviewSlot));
        QCOMPARE(f.bar()->count(), 2);
        const LayoutNode &tree = f.window->currentLayoutView()->tree();
        QVERIFY(tree.ref);
        QCOMPARE(tree.ref->item, itemId);
    }

    // Without profiles, item cells keep their placeholders. With them, an item
    // cell's body is a Placement.
    void itemCellsGetPlacements()
    {
        {
            Fixture f;
            CellFrame *cell = itemCell(f.window->currentLayoutView());
            QVERIFY(cell);
            QVERIFY(!qobject_cast<Placement *>(cell->body()));
        }
        Fixture f(true);
        CellFrame *cell = itemCell(f.window->currentLayoutView());
        QVERIFY(cell);
        auto *placement = qobject_cast<Placement *>(cell->body());
        QVERIFY(placement);
        QCOMPARE(placement->itemId(), cell->ref()->item);
    }

    // A web item's cell shows a web page, in its effective web profile.
    void webItemShowsPage()
    {
        Fixture f(true);
        f.window->show();
        QVERIFY(QTest::qWaitForWindowExposed(f.window.get()));
        f.window->openItem(kAuditTask, kAuditPage);
        CellFrame *cell = itemCell(f.window->currentLayoutView(), kAuditPage);
        QVERIFY(cell);
        auto *placement = qobject_cast<Placement *>(cell->body());
        QVERIFY(placement);
        QTRY_VERIFY(qobject_cast<WebPanel *>(placement->view()));
        auto *panel = qobject_cast<WebPanel *>(placement->view());
        QCOMPARE(panel->view()->url(), kTestPage);
        const QString profileId = profiles::effective(f.model, kAuditTask, kAuditPage);
        QCOMPARE(panel->view()->page()->profile(), f.profiles->profile(profileId));
        QCOMPARE(placement->headerWidget(), panel->toolbar());
    }

    // Deleting a profile in the profiles dialog deletes its logins,
    // after the pages that used it: here a preview tab and a detached window.
    // Those pages are made again in the profile they fall back to. Deleting a
    // group deletes no profile.
    void deletingProfileRemovesItsLogins()
    {
        WarningLog warnings;
        {
            Fixture f(true);
            f.window->show();
            QVERIFY(QTest::qWaitForWindowExposed(f.window.get()));

            // The task's group uses its own profile.
            Profile audit;
            audit.id = QStringLiteral("audit-profile");
            audit.name = QStringLiteral("Audit");
            f.model.setProfile(audit);
            Group group = *f.model.group(f.groupOf(kAuditTask));
            group.profile = audit.id;
            f.model.setGroup(group);

            emit f.window->findChild<Sidebar *>()->openItemInNewRootDashboard(kAuditTask, kAuditPage);
            emit f.bar()->detachRequested(f.bar()->currentIndex());
            DetachedWindow *window = detachedWindows().value(0);
            QVERIFY(window);
            f.window->openItem(kAuditTask, kAuditPage);
            CellFrame *detachedCell = itemCell(window->page(), kAuditPage);
            CellFrame *previewCell = itemCell(f.window->currentLayoutView(), kAuditPage);
            QVERIFY(detachedCell && previewCell);
            auto *detachedPlacement = qobject_cast<Placement *>(detachedCell->body());
            auto *previewPlacement = qobject_cast<Placement *>(previewCell->body());
            QVERIFY(detachedPlacement && previewPlacement);
            QTRY_VERIFY(detachedPlacement->view() && previewPlacement->view());
            const QPointer<QWidget> detachedView = detachedPlacement->view();
            const QPointer<QWidget> previewView = previewPlacement->view();
            QCOMPARE(qobject_cast<WebPanel *>(previewView.data())->view()->page()->profile(),
                     f.profiles->profile(audit.id));

            const QString folder = f.profiles->storagePath(ProfileManager::storageName(audit.id));
            QTRY_VERIFY(QFileInfo::exists(folder));

            f.window->entityActions()->setConfirm([](const QString &) { return true; });
            f.window->entityActions()->setEditProfiles([&audit](ProfilesDialog &dialog) {
                dialog.select(audit.id);
                dialog.deleteSelected();
            });
            f.window->entityActions()->editProfiles();
            QVERIFY(!f.model.profile(audit.id));
            QTRY_VERIFY(!previewView);
            QTRY_VERIFY(!detachedView);
            QTRY_VERIFY_WITH_TIMEOUT(!QFileInfo::exists(folder), 15000);

            // The new pages use Default.
            auto *panel = qobject_cast<WebPanel *>(previewPlacement->view());
            QVERIFY(panel);
            QCOMPARE(panel->view()->page()->profile(), f.profiles->profile(kDefaultProfile));

            // Deleting the group leaves Default's logins alone.
            const QString defaultFolder = f.profiles->storagePath(ProfileManager::storageName(kDefaultProfile));
            QVERIFY(QFileInfo::exists(defaultFolder));
            f.window->entityActions()->removeGroup(group.id);
            QVERIFY(!f.model.group(group.id));
            QTest::qWait(100);
            QVERIFY(QFileInfo::exists(defaultFolder));
        }
        for (const QString &message : std::as_const(WarningLog::messages))
            QVERIFY2(!message.contains(QStringLiteral("profile"), Qt::CaseInsensitive), qPrintable(message));
    }

    // A server that refuses its token gets a line in the warning bar, with a
    // button that opens its settings screen in its profile. A new token
    // there takes the line away.
    void failingServerLine()
    {
        Fixture f;
        Api api(f);
        f.model.setModuleSettings(QStringLiteral("default"), QStringLiteral("srv"), QStringLiteral("general"),
                                  {{QStringLiteral("baseUrl"), QStringLiteral("https://srv.example")}});
        api.secrets.write(srvTokenKey(kSampleWorkspaceId), QStringLiteral("wrong"));
        QVERIFY(f.warningBar()->isHidden());

        ApiClient *client = api.servers.client(QStringLiteral("default"), QStringLiteral("srv"));
        QVERIFY(client);
        bool answered = false;
        client->get(QStringLiteral("api/user"), {}, {}, f.window.get(), [&answered](const ApiReply &) {
            answered = true;
        });
        QTRY_VERIFY(answered);
        QVERIFY(api.servers.failing(QStringLiteral("default"), QStringLiteral("srv")));
        QTRY_COMPARE(f.window->findChildren<QWidget *>(QStringLiteral("failingServer")).size(), 1);
        QVERIFY(!f.warningBar()->isHidden());
        QWidget *line = f.window->findChild<QWidget *>(QStringLiteral("failingServer"));
        QCOMPARE(line->findChild<QLabel *>()->text(), QStringLiteral("Srv in the profile Default refused the token."));

        // "Set Token…" opens the server's screen in that profile.
        QString title;
        f.window->entityActions()->setExec([&title](QDialog &dialog) {
            title = dialog.windowTitle();
            auto *form = dialog.findChild<FieldForm *>(QStringLiteral("editor"));
            auto *token = form ? qobject_cast<QLineEdit *>(form->editorFor(QStringLiteral("token"))) : nullptr;
            if (!token)
                return false;
            // The token is set, and the screen says so without showing it.
            const QString set = QStringLiteral("Set on this device. Type to replace it.");
            if (!QTest::qWaitFor([token, &set] { return token->placeholderText() == set; }))
                return false;
            token->setText(QStringLiteral("right"));
            return true;
        });
        line->findChild<QPushButton *>(QStringLiteral("setToken"))->click();
        QCOMPARE(title, QStringLiteral("Default: Srv General Configuration"));
        QCOMPARE(api.secrets.cached(srvTokenKey(kSampleWorkspaceId)), std::optional<QString>(QStringLiteral("right")));
        QTRY_VERIFY(f.window->findChildren<QWidget *>(QStringLiteral("failingServer")).isEmpty());
        QVERIFY(!api.servers.failing(QStringLiteral("default"), QStringLiteral("srv")));
        QVERIFY(f.warningBar()->isHidden());

        // PlacementServices::openServerSettings does the same; another
        // workspace drops the failing clients.
        client = api.servers.client(QStringLiteral("default"), QStringLiteral("srv"));
        client->get(QStringLiteral("api/user"), {}, {}, nullptr, {});
        QTRY_COMPARE(f.window->findChildren<QWidget *>(QStringLiteral("failingServer")).size(), 1);
        f.window->openSampleData();
        QVERIFY(api.servers.failingServers().isEmpty());
        QTRY_VERIFY(f.window->findChildren<QWidget *>(QStringLiteral("failingServer")).isEmpty());
    }

    // A keychain error shows in the warning bar, once per message.
    void keychainFailure()
    {
        Fixture f;
        Api api(f, std::make_unique<LockedBackend>());
        QVERIFY(f.warningBar()->isHidden());
        bool answered = false;
        api.secrets.read(srvTokenKey(kSampleWorkspaceId), nullptr, [&answered](std::optional<QString>) {
            answered = true;
        });
        QTRY_VERIFY(answered);
        QVERIFY(!f.warningBar()->isHidden());
        QCOMPARE(f.warningText(), QStringLiteral("Could not use the keychain: The keychain is locked"));
        api.secrets.write(srvTokenKey(kSampleWorkspaceId), QStringLiteral("token"));
        QTest::qWait(20);
        QCOMPARE(f.warningText(), QStringLiteral("Could not use the keychain: The keychain is locked"));
    }

    // When another device changed the workspace id, the tokens move to the
    // new id with the logins (decision 0063). The first open only records
    // the id.
    void tokensFollowNewWorkspaceId()
    {
        QTemporaryDir dir;
        QVERIFY(copyFixtures(dir.path()));
        const QString folder = QDir(dir.path()).absolutePath();
        Fixture f;
        Api api(f);
        f.window->openWorkspace(folder);
        QCOMPARE(f.model.workspaceId(), kSampleWorkspaceId);
        QCOMPARE(f.settings.workspaceId(folder), kSampleWorkspaceId);
        api.secrets.write(srvTokenKey(kSampleWorkspaceId), QStringLiteral("t0ken"));
        f.dataStore.flush();

        // Another device gives the workspace a new id.
        const QString newId = QStringLiteral("0b6f2a3c-1d4e-4f5a-8b9c-0d1e2f3a4b5c");
        QFile file(QDir(folder).filePath(QStringLiteral("workspace.mirante")));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QJsonObject workspace = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
        workspace.insert(QStringLiteral("id"), newId);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(QJsonDocument(workspace).toJson());
        file.close();

        f.window->reloadWorkspace();
        QCOMPARE(f.model.workspaceId(), newId);
        QCOMPARE(f.settings.workspaceId(folder), newId);
        // A read waits for the move.
        std::optional<QString> moved;
        bool answered = false;
        api.secrets.read(srvTokenKey(newId), nullptr, [&](std::optional<QString> value) {
            moved = value;
            answered = true;
        });
        QTRY_VERIFY(answered);
        QCOMPARE(moved, std::optional<QString>(QStringLiteral("t0ken")));
        QTRY_COMPARE(api.secrets.state(srvTokenKey(kSampleWorkspaceId)), SecretStore::State::NotSet);
    }

    // Opening a task in the preview makes its items due, so a poll fetches
    // them. Their attention reaches the sidebar.
    void openedTaskIsPolled()
    {
        Fixture f;
        Api api(f);
        QCOMPARE(f.model.task(kOverviewTask)->priority, Priority::Normal);
        Item item;
        item.id = QStringLiteral("srv-item");
        item.type = QStringLiteral("srv.thing");
        item.name = QStringLiteral("Thing");
        Task task = *f.model.task(kOverviewTask);
        task.items.push_back(item);
        f.model.setTask(task);

        SrvThing::fetches = 0;
        api.status.poll();
        QTest::qWait(20);
        QCOMPARE(SrvThing::fetches, 0);

        f.window->openTaskPreview(kOverviewTask);
        api.status.poll();
        QTRY_COMPARE(SrvThing::fetches, 1);
        QCOMPARE(api.status.status(item.id).state, ItemStatus::State::Ok);
        auto *sidebar = f.window->findChild<Sidebar *>();
        QTRY_COMPARE(sidebar->model()->indexOf(kOverviewTask).data(SidebarModel::AttentionRole).toInt(), 1);
    }

    // Focus's "Needs attention first" is kept on this device, and its badges
    // come from the window's Attention.
    void focusAttentionFirstIsKept()
    {
        {
            Fixture f;
            Api api(f);
            QVERIFY(!f.settings.focusAttentionFirst());
            f.window->openFocus();
            auto *toggle = f.window->findChild<QCheckBox *>(QStringLiteral("attentionFirst"));
            QVERIFY(toggle);
            QVERIFY(!toggle->isChecked());
            toggle->click();
            QVERIFY(f.settings.focusAttentionFirst());
        }
        Fixture f;
        f.settings.setFocusAttentionFirst(true);
        f.window->openFocus();
        auto *view = f.window->findChild<FocusView *>();
        QVERIFY(view);
        QVERIFY(view->attentionFirst());
    }

    // Profiles > <profile> > Settings… shows where the profile's data is
    // kept. Without web profiles there is nothing to show.
    void profileSettingsShowStorage()
    {
        {
            Fixture f;
            auto *settings = f.window->findChild<QAction *>(QStringLiteral("profileSettings.default"));
            QVERIFY(settings);
            QVERIFY(!settings->isEnabled());
        }
        Fixture f(true);
        auto *settings = f.window->findChild<QAction *>(QStringLiteral("profileSettings.default"));
        QVERIFY(settings);
        QVERIFY(settings->isEnabled());
        settings->trigger();
        auto *dialog = f.window->findChild<QDialog *>(QStringLiteral("profileSettingsDialog"));
        QVERIFY(dialog);
        QVERIFY(dialog->isVisible());
        const QString folder = f.profiles->storagePath(ProfileManager::storageName(QStringLiteral("default")));
        QCOMPARE(dialog->findChild<QLineEdit *>(QStringLiteral("webDataPath"))->text(),
                 QDir::toNativeSeparators(folder));
        QCOMPARE(dialog->findChild<QLineEdit *>(QStringLiteral("cachePath"))->text(),
                 QDir::toNativeSeparators(QDir(folder).filePath(QStringLiteral("cache"))));
        const QString prefix = keychainEntryName(f.model.workspaceId() + QStringLiteral("/default/"));
        QCOMPARE(dialog->findChild<QLineEdit *>(QStringLiteral("tokenPrefix"))->text(), prefix + QStringLiteral("…"));
        QVERIFY(prefix.startsWith(QStringLiteral("mirante/")));

        // A second opening replaces the first window.
        const QPointer<QDialog> first = dialog;
        settings->trigger();
        QTRY_VERIFY(!first);
        QVERIFY(f.window->findChild<QDialog *>(QStringLiteral("profileSettingsDialog")));
    }

private:
    // The visible detached windows, ignoring those already put back.
    static QList<DetachedWindow *> detachedWindows()
    {
        QList<DetachedWindow *> windows;
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            auto *window = qobject_cast<DetachedWindow *>(widget);
            if (window && window->page())
                windows << window;
        }
        return windows;
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
    TestMainWindow test;
    return QTest::qExec(&test, argc, argv);
}
#include "tst_main_window.moc"
