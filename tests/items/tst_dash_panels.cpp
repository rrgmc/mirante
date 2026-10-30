// Tests the built-in panels' pages with a real bridge over the workspace of
// the status tests: the rows each panel shows, and that a click opens what
// the row names. The look is checked by hand, in docs/plans/m9-checks.md.
// See the M9 plan, part 4.
#include "../api/StatusTestSupport.h"

#include "api/Attention.h"
#include "items/DashBridge.h"
#include "items/ItemTypeRegistry.h"
#include "web/DashSchemeHandler.h"
#include "web/DashView.h"
#include "web/ProfileManager.h"

#include <QApplication>
#include <QLibraryInfo>
#include <QSignalSpy>
#include <QWebEnginePage>

using namespace mirante;
using namespace mirante::statustest;

namespace {

// Starting Chromium for the first page takes a while.
constexpr int kTimeout = 20000;

Controller *registryController = nullptr;

void registerTest(ModuleRegistrar &registrar)
{
    registrar.addItemType(std::make_unique<StatusType>(*registryController));
    registrar.addItemType(std::make_unique<PlainType>());
}

// The value of a JavaScript expression in the page.
QVariant evaluate(QWebEnginePage &page, const QString &script)
{
    QVariant result;
    bool done = false;
    page.runJavaScript(script, [&](const QVariant &value) {
        result = value;
        done = true;
    });
    if (!QTest::qWaitFor([&] { return done; }, kTimeout))
        return {};
    return result;
}

// The data-id of each row, once the panel has drawn as many as expected, or
// the last ones seen if it never does, for the comparison to show.
QStringList rows(QWebEnginePage &page, int expected)
{
    const QString script = QStringLiteral("Array.from(document.querySelectorAll('.row')).map(r => r.dataset.id)");
    QStringList ids;
    (void)QTest::qWaitFor(
        [&] {
            ids = evaluate(page, script).toStringList();
            return ids.size() == expected;
        },
        kTimeout);
    return ids;
}

} // namespace

class TestDashPanels : public QObject
{
    Q_OBJECT

    std::unique_ptr<Fixture> f;
    std::unique_ptr<ItemTypeRegistry> types;
    std::unique_ptr<Attention> attention;
    std::unique_ptr<QTemporaryDir> root;
    std::unique_ptr<ProfileManager> profiles;
    PlacementServices services;
    QStringList opened;

    void give(const QString &itemId, const ItemStatus &status)
    {
        f->controller.next[itemId] = status;
        f->cache->refresh(itemId);
    }

    // A panel of t1, loaded.
    std::unique_ptr<DashView> panel(const QString &name)
    {
        auto *bridge = new DashBridge(services, QStringLiteral("t1"));
        auto view = std::make_unique<DashView>(profiles->appProfile(),
                                               QUrl(QStringLiteral("dash://panels/%1/index.html").arg(name)), bridge);
        bridge->setParent(view.get());
        QSignalSpy loaded(view->page(), &QWebEnginePage::loadFinished);
        if (!loaded.wait(kTimeout) || !loaded.first().first().toBool())
            return nullptr;
        return view;
    }

private slots:
    void init()
    {
        // The debug WebEngine stops on its own checks on the offscreen
        // platform. See docs/web-views.md#debug-builds.
        if (QLibraryInfo::isDebugBuild())
            QSKIP("The debug WebEngine stops on its own checks here; run this test in a release build.");
        f = std::make_unique<Fixture>();
        registryController = &f->controller;
        types = std::make_unique<ItemTypeRegistry>();
        types->registerModule(QStringLiteral("test"), &registerTest);
        attention = std::make_unique<Attention>(f->model, *f->cache, *f->device);
        root = std::make_unique<QTemporaryDir>();
        profiles = std::make_unique<ProfileManager>(root->path());
        services = PlacementServices();
        services.model = &f->model;
        services.types = types.get();
        services.status = f->cache.get();
        services.attention = attention.get();
        opened.clear();
        services.openItem = [this](const QString &taskId, const QString &itemId) {
            opened << taskId + QLatin1Char('/') + itemId;
        };
        services.openTask = [this](const QString &taskId) { opened << taskId; };
    }

    void cleanup()
    {
        profiles.reset();
        root.reset();
        attention.reset();
        types.reset();
        f.reset();
    }

    void taskSummary()
    {
        // The first status records what clears on view; the failure counts.
        give(QStringLiteral("i1"), okStatus(QStringLiteral("Open · pipeline failed"),
                                            {reason(QStringLiteral("Pipeline failed"))}));
        auto view = panel(QStringLiteral("task-summary"));
        QVERIFY(view);
        QWebEnginePage &page = *view->page();

        // Only i1 reports a status; i2 is plain and i3's module is missing.
        QCOMPARE(rows(page, 1), QStringList({QStringLiteral("i1")}));
        QCOMPARE(evaluate(page, QStringLiteral("document.querySelector('.heading').textContent")).toString(),
                 QStringLiteral("t1"));
        QCOMPARE(evaluate(page, QStringLiteral("document.querySelector('.row .detail').textContent")).toString(),
                 QStringLiteral("Open · pipeline failed"));
        QCOMPARE(evaluate(page, QStringLiteral("document.querySelector('.row').classList.contains('attention')"))
                     .toBool(),
                 true);
        QCOMPARE(evaluate(page, QStringLiteral("document.querySelector('.reasons li').textContent")).toString(),
                 QStringLiteral("Pipeline failed"));

        // A new status draws again.
        give(QStringLiteral("i1"), okStatus(QStringLiteral("Merged")));
        QVERIFY(QTest::qWaitFor(
            [&] {
                return evaluate(page, QStringLiteral("document.querySelector('.row .detail').textContent"))
                           .toString()
                    == QStringLiteral("Merged");
            },
            kTimeout));

        evaluate(page, QStringLiteral("document.querySelector('.row').click(); true"));
        QVERIFY(QTest::qWaitFor([&] { return !opened.isEmpty(); }, kTimeout));
        QCOMPARE(opened, QStringList({QStringLiteral("t1/i1")}));
        view.reset();
    }

    void taskSummaryWithoutStatus()
    {
        // t1 with its status item removed.
        Task t1 = *f->model.task(QStringLiteral("t1"));
        t1.items.erase(t1.items.begin());
        f->model.setTask(t1);
        auto view = panel(QStringLiteral("task-summary"));
        QVERIFY(view);
        QWebEnginePage &page = *view->page();
        QVERIFY(QTest::qWaitFor(
            [&] {
                return evaluate(page, QStringLiteral("(document.querySelector('.message') || {}).textContent"))
                           .toString()
                    == QStringLiteral("No items in this task report a status.");
            },
            kTimeout));
        view.reset();
    }

    void groupOverview()
    {
        give(QStringLiteral("i4"), okStatus(QStringLiteral("Open"), {reason(QStringLiteral("pipeline-failed"))}));
        auto view = panel(QStringLiteral("group-overview"));
        QVERIFY(view);
        QWebEnginePage &page = *view->page();

        // t2 High before t1 Normal; no Done, trashed or other group's tasks.
        QCOMPARE(rows(page, 2), QStringList({QStringLiteral("t2"), QStringLiteral("t1")}));
        QCOMPARE(evaluate(page, QStringLiteral("document.querySelector('.heading').textContent")).toString(),
                 QStringLiteral("G1"));
        QCOMPARE(evaluate(page, QStringLiteral("document.querySelector('.row .badge').textContent")).toString(),
                 QStringLiteral("1"));
        QCOMPARE(evaluate(page, QStringLiteral("document.querySelectorAll('.badge').length")).toInt(), 1);
        // The panel's own task is marked.
        QCOMPARE(evaluate(page, QStringLiteral("document.querySelectorAll('.row')[1].querySelector('.pill') !== null"))
                     .toBool(),
                 true);

        evaluate(page, QStringLiteral("document.querySelector('.row').click(); true"));
        QVERIFY(QTest::qWaitFor([&] { return !opened.isEmpty(); }, kTimeout));
        QCOMPARE(opened, QStringList({QStringLiteral("t2")}));
        view.reset();
    }

    // The app's colours reach the page as CSS custom properties.
    void colours()
    {
        auto view = panel(QStringLiteral("group-overview"));
        QVERIFY(view);
        QWebEnginePage &page = *view->page();
        QCOMPARE(rows(page, 2).size(), 2);
        // The page's own colour fills it, also below the rows.
        QCOMPARE(evaluate(page, QStringLiteral("getComputedStyle(document.documentElement).backgroundColor")).toString(),
                 QStringLiteral("rgb(255, 255, 255)"));
        QCOMPARE(evaluate(page, QStringLiteral("document.documentElement.style.getPropertyValue('--priorityUrgent')"))
                     .toString(),
                 QStringLiteral("#d93b3b"));
        view.reset();
    }
};

// The scheme must be registered before the application is made, so this test
// has its own main().
int main(int argc, char *argv[])
{
    dash::registerScheme();
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication app(argc, argv);
    TestDashPanels test;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&test, argc, argv);
}

#include "tst_dash_panels.moc"
