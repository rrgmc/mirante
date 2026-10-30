// Tests QuickAddDialog with fake handlers and finders: reading, text no
// handler takes, a choice of readings, the project matched or asked for,
// related items, Create Task, Cancel, the profile, a workspace without
// projects, and the dialog that adds items to a task.
#include "core/Model.h"
#include "items/ItemTypeRegistry.h"
#include "items/QuickAddRun.h"
#include "ui/QuickAddDialog.h"

#include <mirante/Module.h>
#include <mirante/QuickAdd.h>

#include <QClipboard>
#include <QComboBox>
#include <QGuiApplication>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPointer>
#include <QPushButton>
#include <QSignalSpy>
#include <QTest>

#include <functional>
#include <map>
#include <memory>
#include <vector>

using namespace mirante;

namespace {

// What each fake handler answers, by module id: a proposal, nothing, or,
// without an entry, no answer until the test gives one.
std::map<QString, std::function<void(QuickAddRequest &)>> handlerScripts;
std::map<QString, std::function<void(RelatedItemRequest &)>> finderScripts;

struct Pending
{
    QString moduleId;
    QuickAddRequest *request = nullptr;
    QPointer<QObject> owner;
};
std::vector<Pending> pending;

struct FinderCall
{
    QString moduleId;
    QStringList texts;
    QJsonValue projectSetting;
};
std::vector<FinderCall> finderCalls;
std::vector<QString> profilesRead;      // the profile of each handler request, through its settings

class FakeHandler : public QuickAddHandler
{
public:
    explicit FakeHandler(const QString &moduleId)
        : m_moduleId(moduleId)
    {
    }

    void propose(QuickAddRequest &request) const override
    {
        profilesRead.push_back(request.moduleSettings(QStringLiteral("server")).value(QStringLiteral("name")).toString());
        const auto script = handlerScripts.find(m_moduleId);
        if (script != handlerScripts.end())
            script->second(request);
        else
            pending.push_back({m_moduleId, &request, request.owner()});
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
        finderCalls.push_back({m_moduleId, request.texts(), request.projectSetting(QStringLiteral("project"))});
        const auto script = finderScripts.find(m_moduleId);
        if (script != finderScripts.end())
            script->second(request);
        else
            request.finish({});
    }

private:
    QString m_moduleId;
};

void registerAlpha(ModuleRegistrar &registrar)
{
    registrar.setModuleName(QStringLiteral("Alpha"));
    registrar.addQuickAddHandler(std::make_unique<FakeHandler>(QStringLiteral("alpha")));
    registrar.addRelatedItemFinder(std::make_unique<FakeFinder>(QStringLiteral("alpha")));
}

void registerBeta(ModuleRegistrar &registrar)
{
    registrar.setModuleName(QStringLiteral("Beta"));
    registrar.addQuickAddHandler(std::make_unique<FakeHandler>(QStringLiteral("beta")));
}

QuickAddItem item(const QString &typeId, const QString &name, const QString &key)
{
    return QuickAddItem{typeId, name, QJsonObject{{QStringLiteral("key"), key}}};
}

// A proposal of the alpha module for the project setting value.
QuickAddProposal proposal(const QString &title, const QString &project)
{
    QuickAddProposal p;
    p.title = title;
    p.items.push_back(item(QStringLiteral("alpha.mr"), QStringLiteral("!42"), QStringLiteral("42")));
    p.items.push_back(item(QStringLiteral("alpha.pipeline"), QStringLiteral("main"), QStringLiteral("main")));
    if (!project.isEmpty())
        p.project = ProjectMatch{QStringLiteral("project"), project};
    p.texts << QStringLiteral("feature/PROJ-7");
    return p;
}

std::function<void(QuickAddRequest &)> answer(const QString &title, const QString &project)
{
    return [title, project](QuickAddRequest &request) { request.finish(proposal(title, project)); };
}

std::function<void(QuickAddRequest &)> decline()
{
    return [](QuickAddRequest &request) { request.finish(std::nullopt); };
}

Project project(const QString &id, const QString &groupId, int order, const QString &setting)
{
    Project p;
    p.id = id;
    p.groupId = groupId;
    p.name = QStringLiteral("Project ") + id;
    p.order = order;
    p.moduleSettings = {{QStringLiteral("alpha"), QJsonObject{{QStringLiteral("project"), setting}}}};
    return p;
}

template<typename T>
T *child(QWidget *parent, const char *name)
{
    return parent->findChild<T *>(QString::fromLatin1(name));
}

// The texts of the item list's rows, with a "[x] " or "[ ] " before each item.
QStringList rows(QWidget *dialog)
{
    QStringList texts;
    auto *list = child<QListWidget>(dialog, "items");
    for (int i = 0; i < list->count(); ++i) {
        const QListWidgetItem *row = list->item(i);
        if (row->flags() & Qt::ItemIsUserCheckable)
            texts << (row->checkState() == Qt::Checked ? QStringLiteral("[x] ") : QStringLiteral("[ ] ")) + row->text();
        else
            texts << row->text();
    }
    return texts;
}

} // namespace

class TestQuickAddDialog : public QObject
{
    Q_OBJECT

private:
    Model model;
    ItemTypeRegistry types;
    std::unique_ptr<QuickAddRun> run;

    std::unique_ptr<QuickAddDialog> makeDialog(const QString &profileId = QString())
    {
        auto dialog = std::make_unique<QuickAddDialog>(model, types, *run, profileId);
        dialog->show();
        return dialog;
    }

private slots:
    void initTestCase()
    {
        types.registerModule(QStringLiteral("alpha"), &registerAlpha);
        types.registerModule(QStringLiteral("beta"), &registerBeta);
    }

    void init()
    {
        QGuiApplication::clipboard()->clear();
        handlerScripts.clear();
        finderScripts.clear();
        pending.clear();
        finderCalls.clear();
        profilesRead.clear();
        handlerScripts[QStringLiteral("beta")] = decline();

        model.reset({}, {}, {}, {});
        Group first;
        first.id = QStringLiteral("g1");
        first.name = QStringLiteral("First");
        model.setGroup(first);
        model.setProject(project(QStringLiteral("p1"), QStringLiteral("g1"), 0, QStringLiteral("acme/backend")));
        model.setProject(project(QStringLiteral("p2"), QStringLiteral("g1"), 1, QStringLiteral("acme/frontend")));
        model.setProject(project(QStringLiteral("p3"), QStringLiteral("g1"), 2, QStringLiteral("acme/frontend")));

        Profile work;
        work.id = QStringLiteral("work");
        work.name = QStringLiteral("Work");
        work.moduleSettings = {{QStringLiteral("alpha"),
                                QJsonObject{{QStringLiteral("server"), QJsonObject{{QStringLiteral("name"), QStringLiteral("work")}}}}}};
        model.setProfile(work);

        run = std::make_unique<QuickAddRun>(model, types, nullptr);
    }

    void cleanup() { run.reset(); }

    void readsAndCreates()
    {
        handlerScripts[QStringLiteral("alpha")] = answer(QStringLiteral("Fix login"), QStringLiteral("ACME/Backend"));
        finderScripts[QStringLiteral("alpha")] = [](RelatedItemRequest &request) {
            request.finish({item(QStringLiteral("beta.issue"), QStringLiteral("PROJ-7"), QStringLiteral("PROJ-7"))});
        };
        auto dialog = makeDialog();
        QSignalSpy created(dialog.get(), &QuickAddDialog::taskCreated);
        auto *create = child<QPushButton>(dialog.get(), "create");
        QCOMPARE(dialog->windowTitle(), QStringLiteral("New Task from Link"));
        QCOMPARE(create->text(), QStringLiteral("Create Task"));
        QVERIFY(!create->isEnabled());
        QVERIFY(!child<QLabel>(dialog.get(), "noProjects")->isVisible());

        dialog->setText(QStringLiteral("https://gitlab.example/acme/backend/-/merge_requests/42"));
        QCOMPARE(child<QLineEdit>(dialog.get(), "title")->text(), QStringLiteral("Fix login"));
        QCOMPARE(child<QComboBox>(dialog.get(), "project")->currentData().toString(), QStringLiteral("p1"));
        QVERIFY(!child<QComboBox>(dialog.get(), "readAs")->isVisible());
        QVERIFY(!child<QLabel>(dialog.get(), "status")->isVisible());

        // The finders ran in the matched project, with the title added.
        QCOMPARE(finderCalls.size(), size_t(1));
        QCOMPARE(finderCalls[0].texts, QStringList({QStringLiteral("feature/PROJ-7"), QStringLiteral("Fix login")}));
        QCOMPARE(finderCalls[0].projectSetting.toString(), QStringLiteral("acme/backend"));
        QCOMPARE(rows(dialog.get()), QStringList({QStringLiteral("[x] !42"), QStringLiteral("[x] main"),
                                                  QStringLiteral("Related"), QStringLiteral("[x] PROJ-7")}));

        // Leave the pipeline out, and rename the task.
        child<QListWidget>(dialog.get(), "items")->item(1)->setCheckState(Qt::Unchecked);
        child<QLineEdit>(dialog.get(), "title")->setText(QStringLiteral("Fix the login"));
        QVERIFY(create->isEnabled());
        create->click();

        QCOMPARE(created.count(), 1);
        const QString taskId = created.at(0).at(0).toString();
        QCOMPARE(dialog->createdTaskId(), taskId);
        QCOMPARE(dialog->result(), int(QDialog::Accepted));
        const Task *task = model.task(taskId);
        QVERIFY(task);
        QCOMPARE(task->projectId, QStringLiteral("p1"));
        QCOMPARE(task->title, QStringLiteral("Fix the login"));
        QCOMPARE(task->items.size(), size_t(2));
        QCOMPARE(task->items[0].name, QStringLiteral("!42"));
        QCOMPARE(task->items[1].type, QStringLiteral("beta.issue"));
    }

    void showsReadingThenUnrecognised()
    {
        auto dialog = makeDialog();
        auto *status = child<QLabel>(dialog.get(), "status");
        dialog->setText(QStringLiteral("hello"));
        QCOMPARE(status->text(), QStringLiteral("Reading…"));
        QVERIFY(status->isVisible());
        QCOMPARE(pending.size(), size_t(1));
        pending[0].request->finish(std::nullopt);
        QCOMPARE(status->text(), QStringLiteral("Mirante does not recognise this text."));
        QVERIFY(!child<QLineEdit>(dialog.get(), "title")->isVisible());
        QVERIFY(!child<QPushButton>(dialog.get(), "create")->isEnabled());
    }

    void readsOnReturnAndPaste()
    {
        handlerScripts[QStringLiteral("alpha")] = answer(QStringLiteral("Typed"), QString());
        auto dialog = makeDialog();
        auto *text = child<QLineEdit>(dialog.get(), "text");
        text->setFocus();
        // Typing one character at a time does not read.
        QTest::keyClicks(text, QStringLiteral("PROJ"));
        QVERIFY(profilesRead.empty());
        QTest::keyClick(text, Qt::Key_Return);
        QCOMPARE(profilesRead.size(), size_t(2));
        QCOMPARE(child<QLineEdit>(dialog.get(), "title")->text(), QStringLiteral("Typed"));

        // A paste reads at once.
        text->clear();
        QGuiApplication::clipboard()->setText(QStringLiteral("PROJ-8"));
        text->paste();
        QCOMPARE(profilesRead.size(), size_t(4));
        QVERIFY(dialog->isVisible());
    }

    void choosesBetweenReadings()
    {
        handlerScripts[QStringLiteral("alpha")] = answer(QStringLiteral("From alpha"), QString());
        handlerScripts[QStringLiteral("beta")] = answer(QStringLiteral("From beta"), QString());
        auto dialog = makeDialog();
        dialog->setText(QStringLiteral("PROJ-7"));
        auto *readAs = child<QComboBox>(dialog.get(), "readAs");
        QVERIFY(readAs->isVisible());
        QCOMPARE(readAs->count(), 2);
        QCOMPARE(readAs->itemText(0), QStringLiteral("Alpha"));
        QCOMPARE(readAs->itemText(1), QStringLiteral("Beta"));
        QCOMPARE(child<QLineEdit>(dialog.get(), "title")->text(), QStringLiteral("From alpha"));
        readAs->setCurrentIndex(1);
        QCOMPARE(child<QLineEdit>(dialog.get(), "title")->text(), QStringLiteral("From beta"));
    }

    void asksForTheProject()
    {
        handlerScripts[QStringLiteral("alpha")] = answer(QStringLiteral("Other"), QStringLiteral("acme/other"));
        finderScripts[QStringLiteral("alpha")] = [](RelatedItemRequest &request) {
            const QString setting = request.projectSetting(QStringLiteral("project")).toString();
            request.finish({item(QStringLiteral("beta.issue"), setting, setting)});
        };
        auto dialog = makeDialog();
        dialog->setText(QStringLiteral("x"));
        auto *project = child<QComboBox>(dialog.get(), "project");
        auto *hint = child<QLabel>(dialog.get(), "projectHint");
        auto *create = child<QPushButton>(dialog.get(), "create");
        QCOMPARE(project->currentIndex(), -1);
        QCOMPARE(hint->text(), QStringLiteral("No project matches acme/other"));
        QVERIFY(hint->isVisible());
        QVERIFY(!create->isEnabled());
        QVERIFY(finderCalls.empty());

        // Choosing a project runs the finders; another choice runs them again,
        // in place of the first one's items.
        project->setCurrentIndex(project->findData(QStringLiteral("p2")));
        QVERIFY(create->isEnabled());
        QVERIFY(!hint->isVisible());
        QCOMPARE(rows(dialog.get()).mid(2), QStringList({QStringLiteral("Related"), QStringLiteral("[x] acme/frontend")}));
        project->setCurrentIndex(project->findData(QStringLiteral("p1")));
        QCOMPARE(finderCalls.size(), size_t(2));
        QCOMPARE(rows(dialog.get()).mid(2), QStringList({QStringLiteral("Related"), QStringLiteral("[x] acme/backend")}));
    }

    void severalProjectsMatch()
    {
        handlerScripts[QStringLiteral("alpha")] = answer(QStringLiteral("Front"), QStringLiteral("acme/frontend"));
        auto dialog = makeDialog();
        dialog->setText(QStringLiteral("x"));
        QCOMPARE(child<QComboBox>(dialog.get(), "project")->currentIndex(), -1);
        QCOMPARE(child<QLabel>(dialog.get(), "projectHint")->text(), QStringLiteral("Several projects match"));
        QVERIFY(!child<QPushButton>(dialog.get(), "create")->isEnabled());
    }

    void cancelStopsRequests()
    {
        auto dialog = makeDialog();
        QSignalSpy created(dialog.get(), &QuickAddDialog::taskCreated);
        dialog->setText(QStringLiteral("x"));
        QCOMPARE(pending.size(), size_t(1));
        QVERIFY(run->busy());
        child<QPushButton>(dialog.get(), "cancel")->click();
        QCOMPARE(dialog->result(), int(QDialog::Rejected));
        QVERIFY(!run->busy());
        QTRY_VERIFY(!pending[0].owner);
        QCOMPARE(created.count(), 0);
        QVERIFY(dialog->createdTaskId().isEmpty());
    }

    void usesTheProfile()
    {
        // The given profile, else the workspace's default.
        {
            auto dialog = makeDialog(QStringLiteral("work"));
            QCOMPARE(child<QComboBox>(dialog.get(), "profile")->currentData().toString(), QStringLiteral("work"));
            handlerScripts[QStringLiteral("alpha")] = decline();
            dialog->setText(QStringLiteral("x"));
            QCOMPARE(profilesRead.back(), QString());     // beta has no settings
            QCOMPARE(profilesRead.front(), QStringLiteral("work"));
        }
        profilesRead.clear();
        auto dialog = makeDialog();
        auto *profile = child<QComboBox>(dialog.get(), "profile");
        QCOMPARE(profile->currentData().toString(), model.defaultProfile());
        QCOMPARE(profile->count(), 2);
        dialog->setText(QStringLiteral("x"));
        QCOMPARE(profilesRead.front(), QString());
        // Another profile reads the text again.
        profilesRead.clear();
        profile->setCurrentIndex(profile->findData(QStringLiteral("work")));
        QCOMPARE(profilesRead.front(), QStringLiteral("work"));
    }

    void readsTheClipboard()
    {
        handlerScripts[QStringLiteral("alpha")] = answer(QStringLiteral("From clipboard"), QString());
        QGuiApplication::clipboard()->setText(QStringLiteral(" PROJ-7 "));
        {
            auto dialog = makeDialog();
            QCOMPARE(child<QLineEdit>(dialog.get(), "text")->text(), QStringLiteral("PROJ-7"));
            QCOMPARE(child<QLineEdit>(dialog.get(), "title")->text(), QStringLiteral("From clipboard"));
        }
        // More than one line is not read.
        QGuiApplication::clipboard()->setText(QStringLiteral("one\ntwo"));
        auto dialog = makeDialog();
        QVERIFY(child<QLineEdit>(dialog.get(), "text")->text().isEmpty());
        QGuiApplication::clipboard()->clear();
    }

    void needsAProject()
    {
        model.reset({}, {}, {}, {});
        run = std::make_unique<QuickAddRun>(model, types, nullptr);
        auto dialog = makeDialog();
        QVERIFY(child<QLabel>(dialog.get(), "noProjects")->isVisible());
        QCOMPARE(child<QLabel>(dialog.get(), "noProjects")->text(), QStringLiteral("Create a project first."));
        QVERIFY(!child<QPushButton>(dialog.get(), "create")->isVisible());
    }

    void addsToATask()
    {
        // The task is in p2, whose group uses the work profile, and has the
        // proposal's pipeline already.
        Group second;
        second.id = QStringLiteral("g2");
        second.name = QStringLiteral("Second");
        second.order = 1;
        second.profile = QStringLiteral("work");
        model.setGroup(second);
        Project inSecond = *model.project(QStringLiteral("p2"));
        inSecond.groupId = QStringLiteral("g2");
        model.setProject(inSecond);
        const QString taskId = run->create(QStringLiteral("p2"), QStringLiteral("Fix login"),
                                           {item(QStringLiteral("alpha.pipeline"), QStringLiteral("Pipeline"),
                                                 QStringLiteral("main"))});
        const Task before = *model.task(taskId);

        // A project the text does not match: the task's is used all the same.
        handlerScripts[QStringLiteral("alpha")] = answer(QStringLiteral("Other title"), QStringLiteral("acme/other"));
        finderScripts[QStringLiteral("alpha")] = [](RelatedItemRequest &request) {
            request.finish({item(QStringLiteral("beta.issue"), QStringLiteral("PROJ-7"), QStringLiteral("PROJ-7")),
                            item(QStringLiteral("alpha.pipeline"), QStringLiteral("main"), QStringLiteral("main"))});
        };
        std::unique_ptr<QuickAddDialog> dialog(QuickAddDialog::forTask(model, types, *run, taskId));
        dialog->show();
        QSignalSpy added(dialog.get(), &QuickAddDialog::itemsAdded);
        QSignalSpy created(dialog.get(), &QuickAddDialog::taskCreated);
        auto *create = child<QPushButton>(dialog.get(), "create");
        QCOMPARE(dialog->windowTitle(), QStringLiteral("Add Items from Link: Fix login"));
        QCOMPARE(create->text(), QStringLiteral("Add Items"));
        QVERIFY(create->isVisible());
        QVERIFY(!create->isEnabled());
        QCOMPARE(child<QComboBox>(dialog.get(), "profile")->currentData().toString(), QStringLiteral("work"));

        dialog->setText(QStringLiteral("x"));
        QCOMPARE(profilesRead.front(), QStringLiteral("work"));
        QVERIFY(!child<QLineEdit>(dialog.get(), "title")->isVisible());
        QVERIFY(!child<QComboBox>(dialog.get(), "project")->isVisible());
        QVERIFY(!child<QLabel>(dialog.get(), "projectHint")->isVisible());
        QVERIFY(!child<QLabel>(dialog.get(), "noProjects")->isVisible());
        QVERIFY(child<QListWidget>(dialog.get(), "items")->isVisible());

        // The finders ran at once, in the task's project. The pipeline the
        // task has is not offered, by the proposal or the finder.
        QCOMPARE(finderCalls.size(), size_t(1));
        QCOMPARE(finderCalls[0].projectSetting.toString(), QStringLiteral("acme/frontend"));
        QCOMPARE(rows(dialog.get()), QStringList({QStringLiteral("[x] !42"), QStringLiteral("Related"),
                                                  QStringLiteral("[x] PROJ-7")}));

        QVERIFY(create->isEnabled());
        create->click();
        QCOMPARE(created.count(), 0);
        QCOMPARE(added.count(), 1);
        QCOMPARE(added.at(0).at(0).toString(), taskId);
        const QStringList ids = added.at(0).at(1).toStringList();
        QCOMPARE(dialog->addedItemIds(), ids);
        QVERIFY(dialog->createdTaskId().isEmpty());
        QCOMPARE(dialog->result(), int(QDialog::Accepted));
        const Task *task = model.task(taskId);
        QCOMPARE(task->items.size(), size_t(3));
        QCOMPARE(ids, QStringList({task->items[1].id, task->items[2].id}));
        QCOMPARE(task->items[1].name, QStringLiteral("!42"));
        QCOMPARE(task->items[2].name, QStringLiteral("PROJ-7"));
        QCOMPARE(task->title, QStringLiteral("Fix login"));
        QVERIFY(task->dashboards == before.dashboards);
    }

    void addsToATaskWithoutAProfile()
    {
        // A group without a profile starts in the workspace's default.
        const QString taskId = run->create(QStringLiteral("p1"), QStringLiteral("T"), {});
        std::unique_ptr<QuickAddDialog> dialog(QuickAddDialog::forTask(model, types, *run, taskId));
        QCOMPARE(child<QComboBox>(dialog.get(), "profile")->currentData().toString(), model.defaultProfile());
    }
};

QTEST_MAIN(TestQuickAddDialog)
#include "tst_quick_add_dialog.moc"
