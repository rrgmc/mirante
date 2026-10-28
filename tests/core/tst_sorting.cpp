// Tests the display order and the filters of projects and tasks.
#include "core/Sorting.h"

#include <QTest>

using namespace mirante;

namespace {

Project project(const QString &id, const QString &name, Priority priority = Priority::Normal, int order = 0)
{
    Project p;
    p.id = id;
    p.name = name;
    p.priority = priority;
    p.order = order;
    return p;
}

Task task(const QString &id, const QString &title, Priority priority = Priority::Normal, int order = 0,
          TaskState state = TaskState::Active, bool isDefault = false)
{
    Task t;
    t.id = id;
    t.title = title;
    t.priority = priority;
    t.order = order;
    t.state = state;
    t.isDefault = isDefault;
    return t;
}

template<typename T>
std::vector<const T *> pointers(const std::vector<T> &list)
{
    std::vector<const T *> result;
    for (const T &entry : list)
        result.push_back(&entry);
    return result;
}

QStringList projectIds(const std::vector<const Project *> &list)
{
    QStringList ids;
    for (const Project *p : list)
        ids << p->id;
    return ids;
}

QStringList taskIds(const std::vector<const Task *> &list)
{
    QStringList ids;
    for (const Task *t : list)
        ids << t->id;
    return ids;
}

} // namespace

class TestSorting : public QObject
{
    Q_OBJECT

private slots:
    void projectsByPriorityFirst()
    {
        const std::vector<Project> projects = {
            project("low", "A", Priority::Low, 0),
            project("normal", "A", Priority::Normal, 0),
            project("urgent", "A", Priority::Urgent, 5),
            project("high", "A", Priority::High, 0),
        };
        const SortFilter all{false, false};
        QCOMPARE(projectIds(sortedProjects(pointers(projects), all)),
                 QStringList({"urgent", "high", "normal", "low"}));
    }

    void projectsByOrderWithinPriority()
    {
        const std::vector<Project> projects = {
            project("third", "A", Priority::High, 3),
            project("first", "Z", Priority::High, -1),
            project("second", "M", Priority::High, 2),
        };
        QCOMPARE(projectIds(sortedProjects(pointers(projects), SortFilter{})),
                 QStringList({"first", "second", "third"}));
    }

    void projectsByNameThenId()
    {
        const std::vector<Project> projects = {
            project("b", "beta"),
            project("c", "Alpha"),
            project("a", "alpha"),
            project("d", "Gamma"),
        };
        // Names compare without case; equal names fall back to the id.
        QCOMPARE(projectIds(sortedProjects(pointers(projects), SortFilter{})),
                 QStringList({"a", "c", "b", "d"}));
    }

    void projectBeforeIsStrict()
    {
        const Project p = project("x", "Same");
        QVERIFY(!projectBefore(p, p));
        const Project q = project("y", "Same");
        QVERIFY(projectBefore(p, q) != projectBefore(q, p));
    }

    void inputOrderDoesNotMatter()
    {
        const std::vector<Project> projects = {
            project("1", "One", Priority::Urgent),
            project("2", "Two", Priority::Normal, 1),
            project("3", "Three", Priority::Normal, 1),
            project("4", "Four", Priority::Low),
        };
        std::vector<const Project *> forward = pointers(projects);
        std::vector<const Project *> backward(forward.rbegin(), forward.rend());
        QCOMPARE(projectIds(sortedProjects(forward, SortFilter{})),
                 projectIds(sortedProjects(backward, SortFilter{})));
        QCOMPARE(projectIds(sortedProjects(forward, SortFilter{})), QStringList({"1", "3", "2", "4"}));
    }

    void hideLowProjects()
    {
        const std::vector<Project> projects = {
            project("low", "A", Priority::Low),
            project("normal", "B"),
        };
        QCOMPARE(projectIds(sortedProjects(pointers(projects), SortFilter{true, false})),
                 QStringList({"normal"}));
        QCOMPARE(projectIds(sortedProjects(pointers(projects), SortFilter{false, false})),
                 QStringList({"normal", "low"}));
        QVERIFY(!projectVisible(projects[0], SortFilter{true, false}));
    }

    void hideClosedDoesNotApplyToProjects()
    {
        const Project p = project("p", "P", Priority::Normal);
        QVERIFY(projectVisible(p, SortFilter{false, true}));
    }

    void nullProjectsAreDropped()
    {
        const Project p = project("p", "P");
        QCOMPARE(projectIds(sortedProjects({nullptr, &p, nullptr}, SortFilter{})), QStringList({"p"}));
    }

    void defaultTaskFirst()
    {
        const std::vector<Task> tasks = {
            task("urgent", "Fix", Priority::Urgent, -10),
            task("default", "Default", Priority::Low, 99, TaskState::Active, true),
            task("normal", "Other"),
        };
        QCOMPARE(taskIds(sortedTasks(pointers(tasks), SortFilter{})),
                 QStringList({"default", "urgent", "normal"}));
    }

    void tasksByPriorityOrderTitleId()
    {
        const std::vector<Task> tasks = {
            task("n2", "b", Priority::Normal, 1),
            task("n1", "z", Priority::Normal, 0),
            task("h", "z", Priority::High, 7),
            task("n3", "A", Priority::Normal, 1),
            task("n4", "a", Priority::Normal, 1),
            task("u", "z", Priority::Urgent, 9),
        };
        QCOMPARE(taskIds(sortedTasks(pointers(tasks), SortFilter{})),
                 QStringList({"u", "h", "n1", "n3", "n4", "n2"}));
    }

    void hideClosedTasks()
    {
        const std::vector<Task> tasks = {
            task("active", "A"),
            task("done", "B", Priority::Normal, 0, TaskState::Done),
            task("archived", "C", Priority::Normal, 0, TaskState::Archived),
        };
        QCOMPARE(taskIds(sortedTasks(pointers(tasks), SortFilter{false, true})), QStringList({"active"}));
        QCOMPARE(taskIds(sortedTasks(pointers(tasks), SortFilter{false, false})),
                 QStringList({"active", "done", "archived"}));
    }

    void hideLowTasksKeepsDefault()
    {
        const std::vector<Task> tasks = {
            task("default", "Default", Priority::Low, 0, TaskState::Active, true),
            task("low", "L", Priority::Low),
            task("normal", "N"),
        };
        const SortFilter filter{true, false};
        QCOMPARE(taskIds(sortedTasks(pointers(tasks), filter)), QStringList({"default", "normal"}));
        QVERIFY(taskVisible(tasks[0], filter));
        QVERIFY(!taskVisible(tasks[1], filter));
    }

    void defaultTaskIsNeverHidden()
    {
        const Task done = task("default", "Default", Priority::Low, 0, TaskState::Done, true);
        QVERIFY(taskVisible(done, SortFilter{true, true}));
    }

    void defaultFilterValues()
    {
        const SortFilter filter;
        QVERIFY(!filter.hideLow);
        QVERIFY(filter.hideClosed);
        QVERIFY(!taskVisible(task("d", "D", Priority::Normal, 0, TaskState::Done), filter));
        QVERIFY(taskVisible(task("l", "L", Priority::Low), filter));
    }
};

QTEST_GUILESS_MAIN(TestSorting)
#include "tst_sorting.moc"
