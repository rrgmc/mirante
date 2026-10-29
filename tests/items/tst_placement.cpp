// Tests Placement and its ItemContext with test item types: when the view is
// made, unloading by the ViewPool, making the view again after a change, the
// header widget, and what the context reads from the model.
#include "core/DeviceSettings.h"
#include "core/Model.h"
#include "items/ItemContextImpl.h"
#include "items/ItemTypeRegistry.h"
#include "items/Placement.h"
#include "items/ViewPool.h"
#include "web/ProfileManager.h"
#include "web/WebPanel.h"

#include <mirante/ItemContext.h>
#include <mirante/ItemType.h>

#include <QApplication>
#include <QDir>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QWebEngineView>

#include <memory>

using namespace mirante;

namespace {

// How many views each test type made. Reset before each test.
int viewsMade = 0;

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
// test.page: a real web page of the item's "url".
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
        return m_id == QStringLiteral("test.native") || m_id == QStringLiteral("test.none") ? Rendering::Native
                                                                                          : Rendering::Web;
    }

    QWidget *createView(ItemContext &context, QWidget *parent) const override
    {
        if (m_id == QStringLiteral("test.none"))
            return nullptr;
        ++viewsMade;
        if (m_id == QStringLiteral("test.page"))
            return context.createWebView(QUrl(context.config().value(QStringLiteral("url")).toString()), parent);
        return new FakeView(context.config().value(QStringLiteral("text")).toString(), parent);
    }

private:
    QString m_id;
};

void registerTest(ModuleRegistrar &registrar)
{
    for (const char *id : {"test.native", "test.web", "test.none", "test.page"})
        registrar.addItemType(std::make_unique<TestType>(QString::fromLatin1(id)));
}

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
                      QJsonObject{{QStringLiteral("baseUrl"), QStringLiteral("https://gitlab.example.com")}}}}}};

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

    std::unique_ptr<Placement> make(const QString &itemId)
    {
        auto placement = std::make_unique<Placement>(services, QStringLiteral("t1"), itemId);
        placement->resize(300, 200);
        return placement;
    }

private slots:
    void initTestCase() { types.registerModule(QStringLiteral("test"), &registerTest); }

    void init()
    {
        viewsMade = 0;
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
