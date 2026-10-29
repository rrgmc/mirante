// Tests FocusView on the fixtures: the rows, their order, the group filter,
// following the model's changes, and opening a task.
#include "core/Model.h"
#include "core/Store.h"
#include "ui/FocusView.h"
#include "ui/SidebarModel.h"

#include <QComboBox>
#include <QListView>
#include <QSignalSpy>
#include <QTest>

#include <memory>

using namespace mirante;

namespace {

const QString workGroup = QStringLiteral("6d1b9bb9-f4c3-5677-a4e1-683ba618d4c7");
const QString personalGroup = QStringLiteral("80314e00-c02f-5f8b-977f-0756b76406ea");

// Active tasks of the fixtures, with priority and project priority.
const QString fixLoginTimeout = QStringLiteral("760c8e20-8555-5a2c-bcbe-75e7fd08ca74");   // Urgent, backend High
const QString m2Skeleton = QStringLiteral("03c87069-4db2-5198-a58f-7f63f74b4800");        // Urgent, Mirante Normal
const QString addRateLimiting = QStringLiteral("d71bd575-6d1b-5088-9da0-7d2cebf0d59e");   // High, backend High
const QString rotateTls = QStringLiteral("16d3c8ff-5fa8-5cf0-8090-c62592d1bd96");         // High, infra Low
const QString taxDocuments = QStringLiteral("7306337b-4dae-561d-8b8a-3582fd2321f8");      // High, home Low
const QString backendOverview = QStringLiteral("c34def7a-af34-507f-a44d-770bb60dbe95");   // Normal, backend High
const QString darkMode = QStringLiteral("bd986bec-58f5-57cb-899d-6174cdaa0af7");          // Normal, frontend Normal, order 0
const QString frontendOverview = QStringLiteral("43197d59-2724-5d22-9d34-30c9d35601a0");  // Normal, frontend Normal, order 0
const QString miranteOverview = QStringLiteral("ad58a3eb-2f53-5aa4-85e9-a69e7d65e190");   // Normal, Mirante Normal, order 0
const QString accessibility = QStringLiteral("2038cae6-f1d0-5380-a038-ecc1602c4c5a");     // Normal, frontend Normal, order 1
const QString infraOverview = QStringLiteral("6c78a1cf-df72-550c-b5b7-0c41a424450e");     // Normal, infra Low
const QString homeOverview = QStringLiteral("d71f3227-79a2-55e9-a0d8-429b99432282");      // Normal, home Low
const QString flakyTest = QStringLiteral("02fc9a52-653c-562f-a3a2-6d5a4ca98623");         // Low, backend High
const QString gardenPlan = QStringLiteral("52816988-0a77-589b-8910-13d9b4bf126c");        // Low, home Low

const QStringList allTasks = {fixLoginTimeout, m2Skeleton, addRateLimiting, rotateTls, taxDocuments,
                              backendOverview, darkMode, frontendOverview, miranteOverview, accessibility,
                              infraOverview, homeOverview, flakyTest, gardenPlan};
const QStringList workTasks = {fixLoginTimeout, addRateLimiting, rotateTls, backendOverview, darkMode,
                               frontendOverview, accessibility, infraOverview, flakyTest};
const QStringList personalTasks = {m2Skeleton, taxDocuments, miranteOverview, homeOverview, gardenPlan};

QStringList without(QStringList list, const QString &id)
{
    list.removeAll(id);
    return list;
}

} // namespace

class TestFocusView : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        model = std::make_unique<Model>();
        store::load(QStringLiteral(MIRANTE_FIXTURES_DIR), *model);
    }

    void cleanup() { model.reset(); }

    void rowsInFocusOrder()
    {
        FocusView view(*model);
        QCOMPARE(view.taskIds(), allTasks);
        QVERIFY(view.groupFilter().isEmpty());

        auto *list = view.findChild<QListView *>();
        QVERIFY(list != nullptr);
        const QModelIndex first = list->model()->index(0, 0);
        QCOMPARE(first.data(Qt::DisplayRole).toString(), QStringLiteral("Fix login timeout"));
        QCOMPARE(first.data(SidebarModel::PriorityRole).toInt(), int(Priority::Urgent));
        QCOMPARE(first.data(SidebarModel::KindRole).toInt(), int(SidebarModel::Kind::Task));
        QVERIFY(first.data(Qt::ToolTipRole).toString().contains(QStringLiteral("backend / Work")));
    }

    void groupFilter()
    {
        FocusView view(*model);
        view.setGroupFilter(workGroup);
        QCOMPARE(view.groupFilter(), workGroup);
        QCOMPARE(view.taskIds(), workTasks);
        view.setGroupFilter(personalGroup);
        QCOMPARE(view.taskIds(), personalTasks);
        view.setGroupFilter(QString());
        QCOMPARE(view.taskIds(), allTasks);
    }

    // "All groups", then the groups in manual order. Choosing one filters.
    void groupComboBox()
    {
        FocusView view(*model);
        auto *combo = view.findChild<QComboBox *>();
        QVERIFY(combo != nullptr);
        QCOMPARE(combo->count(), 3);
        QCOMPARE(combo->itemText(0), QStringLiteral("All groups"));
        QCOMPARE(combo->itemText(1), QStringLiteral("Work"));
        QCOMPARE(combo->itemText(2), QStringLiteral("Personal"));

        combo->setCurrentIndex(2);
        QCOMPARE(view.groupFilter(), personalGroup);
        QCOMPARE(view.taskIds(), personalTasks);

        view.setGroupFilter(workGroup);
        QCOMPARE(combo->currentIndex(), 1);
        combo->setCurrentIndex(0);
        QCOMPARE(view.taskIds(), allTasks);
    }

    void unknownGroupListsEverything()
    {
        FocusView view(*model);
        view.setGroupFilter(QStringLiteral("no-such-group"));
        QVERIFY(view.groupFilter().isEmpty());
        QCOMPARE(view.taskIds(), allTasks);
    }

    // A reset with changed entities, as a reload from the data folder does.
    void followsChangedReset()
    {
        FocusView view(*model);
        std::vector<Group> groups;
        std::vector<Project> projects;
        std::vector<Task> tasks;
        for (const Group *group : model->groups()) {
            groups.push_back(*group);
            for (const Project *project : model->projectsOfGroup(group->id)) {
                projects.push_back(*project);
                for (const Task *task : model->tasksOfProject(project->id)) {
                    tasks.push_back(*task);
                    if (task->id == gardenPlan)
                        tasks.back().priority = Priority::Urgent;
                    if (task->id == flakyTest)
                        tasks.back().state = TaskState::Archived;
                }
            }
        }
        model->reset(groups, projects, tasks, {});
        QCOMPARE(view.taskIds().mid(0, 3), QStringList({fixLoginTimeout, m2Skeleton, gardenPlan}));
        QCOMPARE(view.taskIds().size(), allTasks.size() - 1);
        QVERIFY(!view.taskIds().contains(flakyTest));
    }

    void followsTaskChanges()
    {
        FocusView view(*model);

        Task garden = *model->task(gardenPlan);
        garden.priority = Priority::Urgent;
        model->setTask(garden);
        if (model->task(gardenPlan)->priority != Priority::Urgent)
            QSKIP("Model::setTask is still the stub of T0; T1 implements it.");
        QCOMPARE(view.taskIds().mid(0, 3), QStringList({fixLoginTimeout, m2Skeleton, gardenPlan}));

        garden.state = TaskState::Done;
        model->setTask(garden);
        QCOMPARE(view.taskIds(), without(allTasks, gardenPlan));

        model->removeTask(flakyTest);
        QCOMPARE(view.taskIds(), without(without(allTasks, gardenPlan), flakyTest));
    }

    void followsGroupAndProjectChanges()
    {
        FocusView view(*model);
        view.setGroupFilter(personalGroup);
        auto *combo = view.findChild<QComboBox *>();

        Group personal = *model->group(personalGroup);
        personal.name = QStringLiteral("Home life");
        model->setGroup(personal);
        if (model->group(personalGroup)->name != personal.name)
            QSKIP("Model::setGroup is still the stub of T0; T1 implements it.");
        QCOMPARE(combo->itemText(2), QStringLiteral("Home life"));
        QCOMPARE(view.groupFilter(), personalGroup);

        // home becomes Urgent, so its Overview passes the one of Mirante.
        Project home = *model->project(QStringLiteral("a9e5cd89-4e6d-596c-93ae-61d66d4c9c3d"));
        home.priority = Priority::Urgent;
        model->setProject(home);
        QCOMPARE(view.taskIds(),
                 QStringList({m2Skeleton, taxDocuments, homeOverview, miranteOverview, gardenPlan}));

        // Removing the chosen group goes back to every group.
        model->removeGroup(personalGroup);
        QVERIFY(view.groupFilter().isEmpty());
        QCOMPARE(combo->count(), 2);
        QCOMPARE(view.taskIds(), workTasks);
    }

    void followsReset()
    {
        FocusView view(*model);
        model->reset({}, {}, {}, {});
        QVERIFY(view.taskIds().isEmpty());
        store::load(QStringLiteral(MIRANTE_FIXTURES_DIR), *model);
        QCOMPARE(view.taskIds(), allTasks);
    }

    void opensTask()
    {
        FocusView view(*model);
        view.resize(400, 400);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QSignalSpy opened(&view, &FocusView::taskOpened);

        auto *list = view.findChild<QListView *>();
        const QModelIndex third = list->model()->index(2, 0);
        list->setFocus();
        list->setCurrentIndex(third);
        QTest::keyClick(list, Qt::Key_Return);
        QCOMPARE(opened.count(), 1);
        QCOMPARE(opened.at(0).at(0).toString(), addRateLimiting);

        const QPoint centre = list->visualRect(list->model()->index(1, 0)).center();
        // The view handles a double click only on the row of the press before it.
        QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, centre);
        QTest::mouseDClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, centre);
        QCOMPARE(opened.count(), 2);
        QCOMPARE(opened.at(1).at(0).toString(), m2Skeleton);
    }

private:
    std::unique_ptr<Model> model;
};

QTEST_MAIN(TestFocusView)
#include "tst_focus_view.moc"
