// Tests ConflictBar on a temporary workspace: the text of each kind of line,
// the bar appearing when a sync tool makes a copy, "Keep current", "Use other"
// with and without confirmation, and the compare dialog.
#include "core/Edits.h"
#include "core/Model.h"
#include "core/Store.h"
#include "ui/ConflictBar.h"

#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include <memory>

using namespace mirante;

namespace {

// The watcher waits 300 ms before it reports a change.
constexpr int kTimeout = 10000;

const QString groupId = QStringLiteral("6d1b9bb9-f4c3-5677-a4e1-683ba618d4c7");
const QString projectId = QStringLiteral("3dd79dc2-8ec9-5181-b7d0-704a18f494d5");
const QString dashboardId = QStringLiteral("c34def7a-af34-507f-a44d-770bb60dbe95");
const QString unknownId = QStringLiteral("493ef622-9528-5506-b3c2-a1f07b3de3d1");

QByteArray readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}

void writeFile(const QString &path, const QByteArray &content)
{
    QVERIFY(QDir().mkpath(QFileInfo(path).path()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(content), qint64(content.size()));
}

QByteArray taskBytes(const Task &task)
{
    QJsonObject json = task.toJson();
    json.insert(QStringLiteral("schemaVersion"), 1);
    return store::toBytes(json);
}

// The lines the bar shows now; lines replaced by a refresh are hidden until
// they are deleted.
QList<QWidget *> lines(const ConflictBar &bar)
{
    QList<QWidget *> shown;
    for (QWidget *line : bar.findChildren<QWidget *>(QStringLiteral("conflictLine")))
        if (!line->isHidden())
            shown.append(line);
    return shown;
}

QPushButton *button(const ConflictBar &bar, const QString &name)
{
    const QList<QWidget *> shown = lines(bar);
    return shown.isEmpty() ? nullptr : shown.front()->findChild<QPushButton *>(name);
}

QString lineText(const ConflictBar &bar)
{
    const QList<QWidget *> shown = lines(bar);
    if (shown.isEmpty())
        return {};
    auto *label = shown.front()->findChild<QLabel *>(QStringLiteral("conflictText"));
    return label ? label->text() : QString();
}

} // namespace

class TestConflictBar : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void describe_data();
    void describe();
    void appearsForNewCopy();
    void keepCurrent();
    void useOtherConfirmed();
    void useOtherRefused();
    void compare();

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    Task m_task;
    QString m_taskFile;
    QString m_copyFile;
    QByteArray m_currentBytes;
    QByteArray m_copyBytes;

    QString path(const QString &relPath) const { return m_dir->filePath(relPath); }
    void writeCopy() { writeFile(path(m_copyFile), m_copyBytes); }
};

void TestConflictBar::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    QVERIFY(store::createWorkspace(m_dir->path(), QStringLiteral("Test")));
    writeFile(path(QStringLiteral("groups/") + groupId + QStringLiteral(".json")),
              QStringLiteral(R"({"schemaVersion":1,"id":"%1","name":"Work"})").arg(groupId).toUtf8());
    writeFile(path(QStringLiteral("projects/") + projectId + QStringLiteral(".json")),
              QStringLiteral(R"({"schemaVersion":1,"id":"%1","groupId":"%2","name":"backend"})")
                  .arg(projectId, groupId)
                  .toUtf8());

    m_task = edits::newTask(projectId, QStringLiteral("Fix login timeout"));
    m_taskFile = QStringLiteral("tasks/") + m_task.id + QStringLiteral(".json");
    m_copyFile = QStringLiteral("tasks/") + m_task.id + QStringLiteral(".sync-conflict-20261028-101500-ABC.json");
    m_currentBytes = taskBytes(m_task);
    Task other = m_task;
    other.title = QStringLiteral("Fix the login timeout");
    m_copyBytes = taskBytes(other);
    writeFile(path(m_taskFile), m_currentBytes);
}

void TestConflictBar::cleanup()
{
    m_dir.reset();
}

void TestConflictBar::describe_data()
{
    QTest::addColumn<QString>("file");
    QTest::addColumn<QString>("expected");

    QTest::newRow("group") << QStringLiteral("groups/") + groupId + QStringLiteral(".json")
                           << QStringLiteral("Two versions of group Work");
    QTest::newRow("project") << QStringLiteral("projects/") + projectId + QStringLiteral(".json")
                             << QStringLiteral("Two versions of project backend");
    QTest::newRow("task") << QStringLiteral("tasks/") + unknownId + QStringLiteral(".json")
                          << QStringLiteral("Two versions of task Fix login timeout");
    QTest::newRow("dashboard") << QStringLiteral("dashboards/") + dashboardId + QStringLiteral(".json")
                               << QStringLiteral("Two versions of dashboard Morning");
    QTest::newRow("workspace") << QStringLiteral("workspace.mirante")
                               << QStringLiteral("Two versions of the workspace file");
    QTest::newRow("image") << QStringLiteral("files/") + unknownId + QStringLiteral(".png")
                           << QStringLiteral("Two versions of image ") + unknownId + QStringLiteral(".png");
    QTest::newRow("unknown task") << QStringLiteral("tasks/") + dashboardId + QStringLiteral(".json")
                                  << QStringLiteral("Two versions of tasks/") + dashboardId + QStringLiteral(".json");
    QTest::newRow("unknown group") << QStringLiteral("groups/") + unknownId + QStringLiteral(".json")
                                   << QStringLiteral("Two versions of groups/") + unknownId + QStringLiteral(".json");
}

void TestConflictBar::describe()
{
    QFETCH(QString, file);
    QFETCH(QString, expected);

    Group group;
    group.id = groupId;
    group.name = QStringLiteral("Work");
    Project project;
    project.id = projectId;
    project.groupId = groupId;
    project.name = QStringLiteral("backend");
    Task task;
    task.id = unknownId;
    task.projectId = projectId;
    task.title = QStringLiteral("Fix login timeout");
    RootDashboard dashboard;
    dashboard.id = dashboardId;
    dashboard.name = QStringLiteral("Morning");
    Model model;
    model.reset({group}, {project}, {task}, {dashboard});

    Store::Conflict conflict;
    conflict.file = file;
    conflict.copy = file + QStringLiteral(".copy");
    QCOMPARE(ConflictBar::describe(conflict, model), expected);
}

void TestConflictBar::appearsForNewCopy()
{
    Model model;
    Store store(model);
    QVERIFY(store.open(m_dir->path()).empty());
    QWidget window;
    ConflictBar *bar = new ConflictBar(store, model, &window);
    window.show();
    QVERIFY(!bar->isVisible());

    writeCopy();
    QTRY_VERIFY_WITH_TIMEOUT(bar->isVisible(), kTimeout);
    QCOMPARE(lines(*bar).size(), 1);
    QCOMPARE(lineText(*bar), QStringLiteral("Two versions of task Fix login timeout"));
    QVERIFY(button(*bar, QStringLiteral("keepCurrent")));
    QVERIFY(button(*bar, QStringLiteral("useOther")));
    QVERIFY(button(*bar, QStringLiteral("compare")));

    // A renamed task renames its line.
    Task renamed = *model.task(m_task.id);
    renamed.title = QStringLiteral("Login");
    model.setTask(renamed);
    QCOMPARE(lineText(*bar), QStringLiteral("Two versions of task Login"));
}

void TestConflictBar::keepCurrent()
{
    writeCopy();
    Model model;
    Store store(model);
    QVERIFY(store.open(m_dir->path()).empty());
    QWidget window;
    ConflictBar *bar = new ConflictBar(store, model, &window);
    window.show();
    QVERIFY(bar->isVisible());
    QCOMPARE(lines(*bar).size(), 1);

    QTest::mouseClick(button(*bar, QStringLiteral("keepCurrent")), Qt::LeftButton);
    QVERIFY(!QFile::exists(path(m_copyFile)));
    QCOMPARE(readFile(path(m_taskFile)), m_currentBytes);
    QVERIFY(!bar->isVisible());
    QVERIFY(lines(*bar).isEmpty());
    QCOMPARE(model.task(m_task.id)->title, QStringLiteral("Fix login timeout"));
}

void TestConflictBar::useOtherConfirmed()
{
    writeCopy();
    Model model;
    Store store(model);
    QVERIFY(store.open(m_dir->path()).empty());
    QWidget window;
    ConflictBar *bar = new ConflictBar(store, model, &window);
    QStringList questions;
    bar->setConfirm([&questions](const QString &question) {
        questions.append(question);
        return true;
    });
    window.show();

    QTest::mouseClick(button(*bar, QStringLiteral("useOther")), Qt::LeftButton);
    QCOMPARE(questions, QStringList{QStringLiteral(
                            "Replace task Fix login timeout with the other version? The current version is deleted.")});
    QCOMPARE(readFile(path(m_taskFile)), m_copyBytes);
    QVERIFY(!QFile::exists(path(m_copyFile)));
    QCOMPARE(model.task(m_task.id)->title, QStringLiteral("Fix the login timeout"));
    QVERIFY(!bar->isVisible());
}

void TestConflictBar::useOtherRefused()
{
    writeCopy();
    Model model;
    Store store(model);
    QVERIFY(store.open(m_dir->path()).empty());
    QWidget window;
    ConflictBar *bar = new ConflictBar(store, model, &window);
    int asked = 0;
    bar->setConfirm([&asked](const QString &) {
        ++asked;
        return false;
    });
    window.show();

    QTest::mouseClick(button(*bar, QStringLiteral("useOther")), Qt::LeftButton);
    QCOMPARE(asked, 1);
    QCOMPARE(readFile(path(m_taskFile)), m_currentBytes);
    QCOMPARE(readFile(path(m_copyFile)), m_copyBytes);
    QCOMPARE(model.task(m_task.id)->title, QStringLiteral("Fix login timeout"));
    QVERIFY(bar->isVisible());
    QCOMPARE(lines(*bar).size(), 1);
    QCOMPARE(store.conflicts().size(), std::size_t(1));
}

void TestConflictBar::compare()
{
    writeCopy();
    Model model;
    Store store(model);
    QVERIFY(store.open(m_dir->path()).empty());
    QWidget window;
    ConflictBar *bar = new ConflictBar(store, model, &window);
    window.show();

    // The dialog is modal, so it is inspected and answered from a timer.
    QString currentText;
    QString otherText;
    qsizetype currentMarked = 0;
    qsizetype otherMarked = 0;
    QStringList labels;
    bool answered = false;
    QTimer timer;
    timer.setInterval(20);
    connect(&timer, &QTimer::timeout, &timer, [&] {
        auto *dialog = window.findChild<QDialog *>(QStringLiteral("compareDialog"));
        if (!dialog || !dialog->isVisible() || answered)
            return;
        auto *current = dialog->findChild<QPlainTextEdit *>(QStringLiteral("currentVersion"));
        auto *other = dialog->findChild<QPlainTextEdit *>(QStringLiteral("otherVersion"));
        if (current && other) {
            currentText = current->toPlainText();
            otherText = other->toPlainText();
            currentMarked = current->extraSelections().size();
            otherMarked = other->extraSelections().size();
            QVERIFY(current->isReadOnly());
            QVERIFY(other->isReadOnly());
        }
        for (QLabel *label : dialog->findChildren<QLabel *>())
            labels.append(label->text());
        answered = true;
        QTest::mouseClick(dialog->findChild<QPushButton *>(QStringLiteral("compareKeepCurrent")), Qt::LeftButton);
    });
    timer.start();

    QTest::mouseClick(button(*bar, QStringLiteral("compare")), Qt::LeftButton);
    QTRY_VERIFY_WITH_TIMEOUT(answered, kTimeout);
    timer.stop();

    // Both files, pretty-printed, with only the title line marked.
    QCOMPARE(currentText, QString::fromUtf8(m_currentBytes));
    QCOMPARE(otherText, QString::fromUtf8(m_copyBytes));
    QCOMPARE(currentMarked, qsizetype(1));
    QCOMPARE(otherMarked, qsizetype(1));
    QVERIFY(labels.contains(m_taskFile));
    QVERIFY(labels.contains(m_copyFile));

    // "Keep current" in the dialog deleted the copy.
    QTRY_VERIFY_WITH_TIMEOUT(!QFile::exists(path(m_copyFile)), kTimeout);
    QVERIFY(!bar->isVisible());
    QCOMPARE(readFile(path(m_taskFile)), m_currentBytes);
}

QTEST_MAIN(TestConflictBar)
#include "tst_conflict_bar.moc"
