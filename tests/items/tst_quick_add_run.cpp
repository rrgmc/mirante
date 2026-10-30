// Tests QuickAddRun with fake handlers and finders: answers in any order, a
// handler that never finishes, stale answers dropped, projects matched, items
// already offered dropped, and the task that create() makes.
#include "core/Model.h"
#include "items/ItemTypeRegistry.h"
#include "items/QuickAddRun.h"

#include <mirante/Module.h>
#include <mirante/QuickAdd.h>

#include <QPointer>
#include <QSignalSpy>
#include <QTest>

#include <functional>
#include <map>
#include <vector>

using namespace mirante;

namespace {

// What each fake does with a request, by module id. Without an entry, the
// request is kept in pending and left running.
std::map<QString, std::function<void(QuickAddRequest &)>> handlerScripts;
std::map<QString, std::function<void(RelatedItemRequest &)>> finderScripts;

struct PendingHandler
{
    QString moduleId;
    QuickAddRequest *request = nullptr;
    QPointer<QObject> owner;
};
struct PendingFinder
{
    QString moduleId;
    RelatedItemRequest *request = nullptr;
    QPointer<QObject> owner;
};
std::vector<PendingHandler> pendingHandlers;
std::vector<PendingFinder> pendingFinders;

class FakeHandler : public QuickAddHandler
{
public:
    explicit FakeHandler(const QString &moduleId)
        : m_moduleId(moduleId)
    {
    }

    void propose(QuickAddRequest &request) const override
    {
        const auto script = handlerScripts.find(m_moduleId);
        if (script != handlerScripts.end() && script->second)
            script->second(request);
        else
            pendingHandlers.push_back({m_moduleId, &request, request.owner()});
    }

private:
    QString m_moduleId;
};

class FakeFinder : public RelatedItemFinder
{
public:
    explicit FakeFinder(const QString &moduleId)
        : m_moduleId(moduleId)
    {
    }

    void find(RelatedItemRequest &request) const override
    {
        const auto script = finderScripts.find(m_moduleId);
        if (script != finderScripts.end() && script->second)
            script->second(request);
        else
            pendingFinders.push_back({m_moduleId, &request, request.owner()});
    }

private:
    QString m_moduleId;
};

void registerAlpha(ModuleRegistrar &registrar)
{
    registrar.addQuickAddHandler(std::make_unique<FakeHandler>(QStringLiteral("alpha")));
    registrar.addRelatedItemFinder(std::make_unique<FakeFinder>(QStringLiteral("alpha")));
}

void registerBeta(ModuleRegistrar &registrar)
{
    registrar.addQuickAddHandler(std::make_unique<FakeHandler>(QStringLiteral("beta")));
    registrar.addRelatedItemFinder(std::make_unique<FakeFinder>(QStringLiteral("beta")));
}

QuickAddItem item(const QString &typeId, const QString &name, const QJsonObject &config)
{
    return QuickAddItem{typeId, name, config};
}

QuickAddProposal proposal(const QString &title)
{
    QuickAddProposal p;
    p.title = title;
    p.items.push_back(item(QStringLiteral("alpha.mr"), QStringLiteral("!42"), {{QStringLiteral("iid"), 42}}));
    p.texts << QStringLiteral("PROJ-7");
    return p;
}

Project project(const QString &id, const QString &groupId, int order, const QJsonValue &setting)
{
    Project p;
    p.id = id;
    p.groupId = groupId;
    p.name = id;
    p.order = order;
    if (!setting.isUndefined())
        p.moduleSettings = {{QStringLiteral("alpha"), QJsonObject{{QStringLiteral("project"), setting}}}};
    return p;
}

} // namespace

class TestQuickAddRun : public QObject
{
    Q_OBJECT

private:
    Model model;
    ItemTypeRegistry types;

private slots:
    void initTestCase()
    {
        types.registerModule(QStringLiteral("beta"), &registerBeta);
        types.registerModule(QStringLiteral("alpha"), &registerAlpha);
    }

    void init()
    {
        handlerScripts.clear();
        finderScripts.clear();
        pendingHandlers.clear();
        pendingFinders.clear();
        model.reset({}, {}, {}, {});

        Group second;
        second.id = QStringLiteral("g2");
        second.name = QStringLiteral("Second");
        second.order = 1;
        Group first;
        first.id = QStringLiteral("g1");
        first.name = QStringLiteral("First");
        first.order = 0;
        model.setGroup(second);
        model.setGroup(first);
        // In sidebar order: p1, p2 in First, then p3 in Second.
        model.setProject(project(QStringLiteral("p3"), QStringLiteral("g2"), 0, QStringLiteral("acme/backend")));
        model.setProject(project(QStringLiteral("p2"), QStringLiteral("g1"), 1, QStringLiteral("ACME/Backend")));
        model.setProject(project(QStringLiteral("p1"), QStringLiteral("g1"), 0, QStringLiteral("acme/frontend")));
        Project trashed = project(QStringLiteral("p4"), QStringLiteral("g1"), 2, QStringLiteral("acme/frontend"));
        trashed.trashedAt = QStringLiteral("2026-10-01T10:00:00Z");
        model.setProject(trashed);
        model.setProject(project(QStringLiteral("p5"), QStringLiteral("g2"), 1, 7));

        Profile work;
        work.id = QStringLiteral("work");
        work.name = QStringLiteral("Work");
        work.moduleSettings = {{QStringLiteral("alpha"),
                                QJsonObject{{QStringLiteral("server"), QJsonObject{{QStringLiteral("url"), QStringLiteral("https://a.example")}}}}}};
        model.setProfile(work);
    }

    void matchesProjects()
    {
        const QString alpha = QStringLiteral("alpha");
        auto match = [](const QJsonValue &value) {
            return std::optional<ProjectMatch>(ProjectMatch{QStringLiteral("project"), value});
        };
        // Several, in sidebar order, without case.
        QCOMPARE(QuickAddRun::matchProjects(model, alpha, match(QStringLiteral("Acme/backend"))),
                 QStringList({QStringLiteral("p2"), QStringLiteral("p3")}));
        // One: the project in the trash is left out.
        QCOMPARE(QuickAddRun::matchProjects(model, alpha, match(QStringLiteral("acme/frontend"))),
                 QStringList({QStringLiteral("p1")}));
        // None.
        QVERIFY(QuickAddRun::matchProjects(model, alpha, match(QStringLiteral("acme/other"))).isEmpty());
        // A number matches a number.
        QCOMPARE(QuickAddRun::matchProjects(model, alpha, match(7)), QStringList({QStringLiteral("p5")}));
        // Another module's settings, a missing key, and no match at all.
        QVERIFY(QuickAddRun::matchProjects(model, QStringLiteral("beta"), match(QStringLiteral("acme/backend"))).isEmpty());
        QVERIFY(QuickAddRun::matchProjects(model, alpha, ProjectMatch{QStringLiteral("other"), QStringLiteral("x")}).isEmpty());
        QVERIFY(QuickAddRun::matchProjects(model, alpha, std::nullopt).isEmpty());
    }

    void answersInAnyOrder()
    {
        QuickAddRun run(model, types, nullptr);
        QSignalSpy proposed(&run, &QuickAddRun::proposed);
        QSignalSpy done(&run, &QuickAddRun::proposalsDone);

        run.propose(QStringLiteral("  PROJ-7 \n"), QStringLiteral("work"));
        QCOMPARE(pendingHandlers.size(), size_t(2));
        QVERIFY(run.busy());
        QCOMPARE(done.count(), 0);

        // By module id: alpha first.
        QuickAddRequest *alpha = pendingHandlers[0].request;
        QuickAddRequest *beta = pendingHandlers[1].request;
        QCOMPARE(pendingHandlers[0].moduleId, QStringLiteral("alpha"));
        QCOMPARE(alpha->text(), QStringLiteral("PROJ-7"));
        QCOMPARE(alpha->moduleSettings(QStringLiteral("server")).value(QStringLiteral("url")).toString(),
                 QStringLiteral("https://a.example"));
        QVERIFY(beta->moduleSettings(QStringLiteral("server")).isEmpty());
        QVERIFY(!alpha->server());
        QVERIFY(alpha->owner());

        beta->finish(proposal(QStringLiteral("From beta")));
        QCOMPARE(proposed.count(), 1);
        QCOMPARE(done.count(), 0);
        alpha->finish(proposal(QStringLiteral("From alpha")));
        QCOMPARE(proposed.count(), 2);
        QCOMPARE(done.count(), 1);
        QVERIFY(!run.busy());

        const auto first = proposed.at(0).at(0).value<QuickAddRun::Proposal>();
        const auto second = proposed.at(1).at(0).value<QuickAddRun::Proposal>();
        QCOMPARE(first.moduleId, QStringLiteral("beta"));
        QCOMPARE(first.proposal.title, QStringLiteral("From beta"));
        QCOMPARE(second.moduleId, QStringLiteral("alpha"));

        // The requests go once the event loop runs.
        QTRY_VERIFY(!pendingHandlers[0].owner && !pendingHandlers[1].owner);
    }

    void declinesAndAnswersAtOnce()
    {
        handlerScripts[QStringLiteral("alpha")] = [](QuickAddRequest &request) { request.finish(std::nullopt); };
        handlerScripts[QStringLiteral("beta")] = [](QuickAddRequest &request) {
            request.finish(proposal(QStringLiteral("At once")));
            // A second finish is ignored.
            request.finish(proposal(QStringLiteral("Twice")));
        };
        QuickAddRun run(model, types, nullptr);
        QSignalSpy proposed(&run, &QuickAddRun::proposed);
        QSignalSpy done(&run, &QuickAddRun::proposalsDone);
        run.propose(QStringLiteral("x"), QString());
        QCOMPARE(proposed.count(), 1);
        QCOMPARE(done.count(), 1);
        QVERIFY(!run.busy());
    }

    void timesOut()
    {
        handlerScripts[QStringLiteral("beta")] = [](QuickAddRequest &request) {
            request.finish(proposal(QStringLiteral("Quick")));
        };
        QuickAddRun run(model, types, nullptr);
        run.setTimeout(50);
        QSignalSpy proposed(&run, &QuickAddRun::proposed);
        QSignalSpy done(&run, &QuickAddRun::proposalsDone);
        run.propose(QStringLiteral("x"), QString());
        QCOMPARE(pendingHandlers.size(), size_t(1));
        QCOMPARE(done.count(), 0);
        // alpha never finishes: it counts as nothing, and its request goes.
        QTRY_COMPARE(done.count(), 1);
        QCOMPARE(proposed.count(), 1);
        QTRY_VERIFY(!pendingHandlers[0].owner);
        QVERIFY(!run.busy());
    }

    void dropsStaleAnswers()
    {
        QuickAddRun run(model, types, nullptr);
        QSignalSpy proposed(&run, &QuickAddRun::proposed);
        QSignalSpy done(&run, &QuickAddRun::proposalsDone);

        run.propose(QStringLiteral("old"), QString());
        QCOMPARE(pendingHandlers.size(), size_t(2));
        const std::vector<PendingHandler> old = pendingHandlers;
        pendingHandlers.clear();
        run.propose(QStringLiteral("new"), QString());
        QCOMPARE(pendingHandlers.size(), size_t(2));

        // The old run's answers are dropped, and its requests go.
        old[0].request->finish(proposal(QStringLiteral("Old")));
        QCOMPARE(proposed.count(), 0);
        QTRY_VERIFY(!old[0].owner && !old[1].owner);
        QVERIFY(pendingHandlers[0].owner);

        // cancel() stops the new run too.
        run.cancel();
        QVERIFY(!run.busy());
        pendingHandlers[1].request->finish(proposal(QStringLiteral("New")));
        QCOMPARE(proposed.count(), 0);
        QCOMPARE(done.count(), 0);
        QTRY_VERIFY(!pendingHandlers[0].owner && !pendingHandlers[1].owner);
    }

    void findsRelatedItems()
    {
        QuickAddRun run(model, types, nullptr);
        QSignalSpy found(&run, &QuickAddRun::found);
        QSignalSpy done(&run, &QuickAddRun::findingDone);

        const QuickAddRun::Proposal handled{QStringLiteral("alpha"), proposal(QStringLiteral("Fix login"))};
        run.findRelated(handled, QStringLiteral("  Fix the login "), QStringLiteral("p2"), QStringLiteral("work"));
        QCOMPARE(pendingFinders.size(), size_t(2));
        QVERIFY(run.busy());

        RelatedItemRequest *alpha = pendingFinders[0].request;
        RelatedItemRequest *beta = pendingFinders[1].request;
        QCOMPARE(alpha->texts(), QStringList({QStringLiteral("PROJ-7"), QStringLiteral("Fix the login")}));
        QCOMPARE(alpha->projectSetting(QStringLiteral("project")).toString(), QStringLiteral("ACME/Backend"));
        QVERIFY(beta->projectSetting(QStringLiteral("project")).isUndefined());
        QCOMPARE(alpha->moduleSettings(QStringLiteral("server")).value(QStringLiteral("url")).toString(),
                 QStringLiteral("https://a.example"));
        QCOMPARE(alpha->items().size(), size_t(1));

        const QuickAddItem issue = item(QStringLiteral("beta.issue"), QStringLiteral("PROJ-7"),
                                        {{QStringLiteral("key"), QStringLiteral("PROJ-7")}});
        // beta repeats the proposal's item, and adds an issue twice.
        beta->finish({handled.proposal.items.front(), issue, issue});
        QCOMPARE(found.count(), 1);
        auto items = found.at(0).at(0).value<std::vector<QuickAddItem>>();
        QCOMPARE(items.size(), size_t(1));
        QCOMPARE(items.front().typeId, QStringLiteral("beta.issue"));
        QCOMPARE(alpha->items().size(), size_t(2));

        // alpha repeats beta's issue, with a new name, and adds a pipeline.
        QuickAddItem renamed = issue;
        renamed.name = QStringLiteral("Other name");
        const QuickAddItem pipeline = item(QStringLiteral("alpha.pipeline"), QStringLiteral("main"),
                                           {{QStringLiteral("ref"), QStringLiteral("main")}});
        alpha->finish({renamed, pipeline});
        QCOMPARE(found.count(), 2);
        items = found.at(1).at(0).value<std::vector<QuickAddItem>>();
        QCOMPARE(items.size(), size_t(1));
        QCOMPARE(items.front().typeId, QStringLiteral("alpha.pipeline"));
        QCOMPARE(done.count(), 1);
        QVERIFY(!run.busy());
    }

    void newSearchDropsTheOld()
    {
        QuickAddRun run(model, types, nullptr);
        QSignalSpy found(&run, &QuickAddRun::found);
        QSignalSpy done(&run, &QuickAddRun::findingDone);
        const QuickAddRun::Proposal handled{QStringLiteral("alpha"), proposal(QStringLiteral("T"))};
        run.findRelated(handled, QString(), QStringLiteral("p1"), QString());
        const std::vector<PendingFinder> old = pendingFinders;
        pendingFinders.clear();
        run.findRelated(handled, QString(), QStringLiteral("p2"), QString());
        old[0].request->finish({item(QStringLiteral("alpha.x"), QStringLiteral("x"), {})});
        QCOMPARE(found.count(), 0);
        QTRY_VERIFY(!old[0].owner && !old[1].owner);
        // Without a title, the texts are the proposal's.
        QCOMPARE(pendingFinders[0].request->texts(), QStringList({QStringLiteral("PROJ-7")}));
        pendingFinders[0].request->finish({});
        pendingFinders[1].request->finish({});
        QCOMPARE(found.count(), 0);
        QCOMPARE(done.count(), 1);
    }

    void createsTheTask()
    {
        QuickAddRun run(model, types, nullptr);
        Task existing = Task();
        existing.id = QStringLiteral("t0");
        existing.projectId = QStringLiteral("p1");
        existing.order = 4;
        model.setTask(existing);

        const std::vector<QuickAddItem> items = {
            item(QStringLiteral("alpha.mr"), QStringLiteral("!42"), {{QStringLiteral("iid"), 42}}),
            item(QStringLiteral("beta.issue"), QStringLiteral("PROJ-7"), {{QStringLiteral("key"), QStringLiteral("PROJ-7")}}),
        };
        const QString id = run.create(QStringLiteral("p1"), QStringLiteral("Fix login"), items);
        QVERIFY(!id.isEmpty());
        const Task *task = model.task(id);
        QVERIFY(task);
        QCOMPARE(task->projectId, QStringLiteral("p1"));
        QCOMPARE(task->title, QStringLiteral("Fix login"));
        QCOMPARE(task->state, TaskState::Active);
        QCOMPARE(task->priority, Priority::Normal);
        QCOMPARE(task->order, 5);
        QCOMPARE(task->items.size(), size_t(2));
        QCOMPARE(task->items[0].type, QStringLiteral("alpha.mr"));
        QCOMPARE(task->items[0].name, QStringLiteral("!42"));
        QCOMPARE(task->items[0].config.value(QStringLiteral("iid")).toInt(), 42);
        QVERIFY(task->items[0].profile.isEmpty());
        QCOMPARE(task->items[1].type, QStringLiteral("beta.issue"));

        QCOMPARE(task->dashboards.size(), size_t(1));
        QCOMPARE(task->defaultDashboardId, task->dashboards.front().id);
        const LayoutNode &root = task->dashboards.front().layout;
        QCOMPARE(root.kind, LayoutNode::Kind::Tabs);
        QCOMPARE(root.current, 0);
        QCOMPARE(root.children.size(), size_t(2));
        QCOMPARE(root.titles, std::vector<QString>({QStringLiteral("!42"), QStringLiteral("PROJ-7")}));
        QCOMPARE(root.children[0].ref->item, task->items[0].id);
        QCOMPARE(root.children[1].ref->item, task->items[1].id);

        // One item fills the cell; none leaves it empty.
        const Task *one = model.task(run.create(QStringLiteral("p1"), QStringLiteral("One"), {items.front()}));
        QVERIFY(one);
        QCOMPARE(one->dashboards.front().layout.kind, LayoutNode::Kind::Cell);
        QCOMPARE(one->dashboards.front().layout.ref->item, one->items.front().id);
        const Task *none = model.task(run.create(QStringLiteral("p1"), QStringLiteral("None"), {}));
        QVERIFY(none);
        QVERIFY(none->items.empty());
        QVERIFY(!none->dashboards.front().layout.ref);

        QVERIFY(run.create(QStringLiteral("missing"), QStringLiteral("x"), items).isEmpty());
    }
};

QTEST_MAIN(TestQuickAddRun)
#include "tst_quick_add_run.moc"
