// Tests EntityActions on the fixtures: creating, editing and deleting each kind
// of entity through its dialog, the refusals (a cancelled dialog or
// confirmation, the last dashboard), priority and state, moves, the trash, and
// the commands each kind of sidebar row gets in its context menu. Also the
// tokens of settings screens: written, removed, shown as set or not, and
// removed with their profile.
#include "api/SecretBackend.h"
#include "api/SecretStore.h"
#include "core/DeviceSettings.h"
#include "core/GroupColours.h"
#include "core/Model.h"
#include "core/Store.h"
#include "items/ItemTypeRegistry.h"
#include "layout/Presets.h"
#include "ui/DashboardDialog.h"
#include "ui/EntityActions.h"
#include "ui/FieldForm.h"
#include "ui/GroupDialog.h"
#include "ui/ImagePaste.h"
#include "ui/ImagePasteDialog.h"
#include "ui/ItemDialog.h"
#include "ui/ProfilesDialog.h"
#include "ui/ProjectDialog.h"
#include "ui/TaskDialog.h"

#include <mirante/ItemEditor.h>
#include <mirante/Module.h>

#include <QAction>
#include <QDateTime>
#include <QImage>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QMenu>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <functional>
#include <memory>

using namespace mirante;

namespace {

const QString workGroup = QStringLiteral("6d1b9bb9-f4c3-5677-a4e1-683ba618d4c7");
const QString personalGroup = QStringLiteral("80314e00-c02f-5f8b-977f-0756b76406ea");
const QString backendProject = QStringLiteral("3dd79dc2-8ec9-5181-b7d0-704a18f494d5");
const QString frontendProject = QStringLiteral("fb99f6f7-be6d-5dd8-b3f2-2b09f71652a8");
const QString backendFolder = QStringLiteral("622f4d51-b29b-5213-82cf-6a85939c8b06");
const QString deployFolder = QStringLiteral("e11c5dbc-ce60-59ee-94f8-15b28d8e5ee1");
const QString fixLoginTimeout = QStringLiteral("760c8e20-8555-5a2c-bcbe-75e7fd08ca74");
const QString fixLoginWorkDashboard = QStringLiteral("e7c480bf");      // prefix; see dashboardId()
const QString fixLoginReviewDashboard = QStringLiteral("ece7e850");    // prefix
const QString backendOverview = QStringLiteral("c34def7a-af34-507f-a44d-770bb60dbe95");
const QString upgradeGo = QStringLiteral("493ef622-9528-5506-b3c2-a1f07b3de3d1");
const QString notesItem = QStringLiteral("11039501");                  // prefix, in fixLoginTimeout

const QString plainType = QStringLiteral("testmod.plain");
const QString editorType = QStringLiteral("testmod.editor");

// The folders the last editor of editorType was given.
std::vector<std::pair<QString, QString>> lastFolders;

// A type edited by a form with one optional field.
class PlainType : public ItemType
{
public:
    QString id() const override { return plainType; }
    QString name() const override { return QStringLiteral("Plain"); }
    QIcon icon() const override { return {}; }

    std::vector<Field> fields() const override
    {
        Field note;
        note.key = QStringLiteral("note");
        note.label = QStringLiteral("Note");
        note.defaultValue = QStringLiteral("hello");
        return {note};
    }
};

class RecordingEditor : public ItemEditor
{
public:
    using ItemEditor::ItemEditor;
    void setConfig(const QJsonObject &config) override { value = config; }
    QJsonObject config() const override { return value; }
    QJsonObject value;
};

// A type with its own editor, which records the context it is given.
class EditorType : public ItemType
{
public:
    QString id() const override { return editorType; }
    QString name() const override { return QStringLiteral("Own editor"); }
    QIcon icon() const override { return {}; }

    ItemEditor *createEditor(const EditorContext &context, QWidget *parent) const override
    {
        lastFolders = context.folders;
        return new RecordingEditor(parent);
    }
};

void registerTestModule(ModuleRegistrar &registrar)
{
    registrar.addItemType(std::make_unique<PlainType>());
    registrar.addItemType(std::make_unique<EditorType>());
}

// A server screen with a token, as GitLab's and Jira's have.
class TokenScreen : public SettingsScreen
{
public:
    QString id() const override { return QStringLiteral("general"); }
    QString name() const override { return QStringLiteral("General Configuration"); }
    std::vector<Field> fields() const override
    {
        Field server;
        server.key = QStringLiteral("server");
        server.label = QStringLiteral("Server");
        server.type = Field::Type::Url;
        Field token;
        token.key = QStringLiteral("token");
        token.label = QStringLiteral("Token");
        token.type = Field::Type::Secret;
        return {server, token};
    }
};

void registerTokenModule(ModuleRegistrar &registrar)
{
    registrar.setModuleName(QStringLiteral("Tokens"));
    registrar.addSettingsScreen(std::make_unique<TokenScreen>());
}

const QString fixtureWorkspaceId = QStringLiteral("5a3e9c1d-7b2f-5e48-a6d0-3c8f1b2e4d97");
const QString defaultProfile = QStringLiteral("default");
const QString workProfile = QStringLiteral("3f6c2a8e-5d1b-4c7a-9e2f-8b0d4a6c1e93");

QString tokenKey(const QString &profileId)
{
    return secretKey(fixtureWorkspaceId, profileId, QStringLiteral("tokens"), QStringLiteral("token"));
}

// The token line of the settings dialog's form.
QLineEdit *tokenLine(QDialog &dialog)
{
    auto *form = dialog.findChild<FieldForm *>(QStringLiteral("editor"));
    return form ? qobject_cast<QLineEdit *>(form->editorFor(QStringLiteral("token"))) : nullptr;
}

// The full id of the dashboard or item of a task whose id starts with prefix.
QString dashboardId(const Model &model, const QString &taskId, const QString &prefix)
{
    for (const Dashboard &dashboard : model.task(taskId)->dashboards) {
        if (dashboard.id.startsWith(prefix))
            return dashboard.id;
    }
    return {};
}

QString itemId(const Model &model, const QString &taskId, const QString &prefix)
{
    for (const Item &item : model.task(taskId)->items) {
        if (item.id.startsWith(prefix))
            return item.id;
    }
    return {};
}

// The texts of the menu's actions, without separators; a submenu by its title.
QStringList texts(const QMenu &menu)
{
    QStringList result;
    for (const QAction *action : menu.actions()) {
        if (!action->isSeparator())
            result << action->text();
    }
    return result;
}

QAction *actionNamed(const QMenu &menu, const QString &text)
{
    for (QAction *action : menu.actions()) {
        if (action->text() == text)
            return action;
    }
    return nullptr;
}

// The text of the checked action of the submenu with title.
QString checkedIn(const QMenu &menu, const QString &title)
{
    const QAction *action = actionNamed(menu, title);
    if (!action || !action->menu())
        return {};
    QString checked;
    for (const QAction *entry : action->menu()->actions()) {
        if (entry->isChecked())
            checked += entry->text();
    }
    return checked;
}

} // namespace

class TestEntityActions : public QObject
{
    Q_OBJECT

    Model model;
    ItemTypeRegistry types;
    std::unique_ptr<Store> store;
    std::unique_ptr<QTemporaryDir> dir;
    std::unique_ptr<QSettings> qsettings;
    std::unique_ptr<DeviceSettings> settings;
    std::unique_ptr<EntityActions> actions;

    // Fills the dialog in before it is accepted; accept is false to cancel.
    std::function<void(QDialog &)> fill;
    bool accept = true;
    int execCount = 0;
    // The confirmations asked, and the answer given.
    QStringList questions;
    bool answer = true;
    // The keyboard modifiers a menu command sees.
    Qt::KeyboardModifiers modifiers = Qt::NoModifier;
    // The image on the clipboard, null for none.
    QImage clipboard;

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void newGroup();
    void newGroupWithItsOwnProfile();
    void newProject();
    void newTask();
    void newItem();
    void newDashboard();
    void cancelledCreation();
    void editing();
    void removeProjectAndGroup();
    void removeTaskItemDashboard();
    void lastDashboard();
    void priorityAndState();
    void moves();
    void menus();
    void trashAndRestore();
    void restoreTaskRestoresProject();
    void deleteMenuMovesToTrash();
    void shiftDeletesPermanently();
    void emptyTrash();
    void trashMenus();
    void pasteImageMenu();
    void typedTokenIsWritten();
    void tokenStateAndRemove();
    void deletingProfileRemovesTokens();
};

void TestEntityActions::initTestCase()
{
    types.registerModule(QStringLiteral("testmod"), registerTestModule);
    types.registerModule(QStringLiteral("tokens"), registerTokenModule);
    store = std::make_unique<Store>(model);
}

void TestEntityActions::init()
{
    QVERIFY(store::load(QStringLiteral(MIRANTE_FIXTURES_DIR), model).empty());
    QVERIFY(model.project(backendProject));
    dir = std::make_unique<QTemporaryDir>();
    QVERIFY(dir->isValid());
    qsettings = std::make_unique<QSettings>(dir->filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
    settings = std::make_unique<DeviceSettings>(qsettings.get());
    actions = std::make_unique<EntityActions>(model, *store, types, *settings, nullptr);

    fill = nullptr;
    accept = true;
    execCount = 0;
    questions.clear();
    answer = true;
    actions->setExec([this](QDialog &dialog) {
        ++execCount;
        if (!accept)
            return false;
        if (fill)
            fill(dialog);
        dialog.accept();
        return dialog.result() == QDialog::Accepted;
    });
    actions->setConfirm([this](const QString &question) {
        questions << question;
        return answer;
    });
    modifiers = Qt::NoModifier;
    actions->setModifiers([this] { return modifiers; });
    clipboard = QImage();
    actions->setClipboardImage([this] { return clipboard; });
}

void TestEntityActions::cleanup()
{
    actions.reset();
    settings.reset();
    qsettings.reset();
    dir.reset();
}

void TestEntityActions::newGroup()
{
    fill = [](QDialog &dialog) {
        auto *group = qobject_cast<GroupDialog *>(&dialog);
        QVERIFY(group);
        group->setName(QStringLiteral("Office"));
    };
    const std::size_t profilesBefore = model.profiles().size();
    const QString id = actions->newGroup();
    QVERIFY(!id.isEmpty());
    const Group *group = model.group(id);
    QVERIFY(group);
    QCOMPARE(group->name, QStringLiteral("Office"));
    QCOMPARE(group->order, 2);   // after Work (0) and Personal (1)
    // The first palette colour that Work (its fallback) and Personal (its own)
    // do not use, stored in the group.
    QVERIFY(group->colour != groupcolours::of(*model.group(QStringLiteral("6d1b9bb9-f4c3-5677-a4e1-683ba618d4c7"))));
    QVERIFY(group->colour != QStringLiteral("#22c55e"));
    QCOMPARE(group->colour, groupcolours::palette().at(0));
    // Other groups exist, so a new profile was offered, unticked: none made.
    QVERIFY(group->profile.isEmpty());
    QCOMPARE(model.profiles().size(), profilesBefore);
}

// "Create a new profile for this group" makes a profile named as asked and
// gives it to the group. A name in use is refused.
void TestEntityActions::newGroupWithItsOwnProfile()
{
    QString suggested;
    fill = [&suggested](QDialog &dialog) {
        auto *group = qobject_cast<GroupDialog *>(&dialog);
        QVERIFY(group);
        group->setName(QStringLiteral("Office"));
        suggested = group->newProfileName();
        group->setCreatesProfile(true);
    };
    const std::size_t before = model.profiles().size();
    const QString id = actions->newGroup();
    QVERIFY(!id.isEmpty());
    QCOMPARE(model.profiles().size(), before + 1);
    const Group *group = model.group(id);
    QVERIFY(group && !group->profile.isEmpty());
    const Profile *profile = model.profile(group->profile);
    QVERIFY(profile);
    QCOMPARE(profile->name, QStringLiteral("Office"));

    // A name already used is refused, so OK does nothing.
    fill = [](QDialog &dialog) {
        auto *group = qobject_cast<GroupDialog *>(&dialog);
        group->setName(QStringLiteral("Lab"));
        group->setCreatesProfile(true);
        group->setNewProfileName(QStringLiteral("office"));
    };
    QVERIFY(actions->newGroup().isEmpty());
    QCOMPARE(model.profiles().size(), before + 1);
}

void TestEntityActions::newProject()
{
    Folder web;
    web.id = QStringLiteral("web");
    web.name = QStringLiteral("web");
    web.main = true;
    fill = [&web](QDialog &dialog) {
        auto *project = qobject_cast<ProjectDialog *>(&dialog);
        QVERIFY(project);
        QCOMPARE(project->value().groupId, workGroup);
        project->setName(QStringLiteral("api"));
        project->setFolders({web}, {{QStringLiteral("web"), QStringLiteral("D:/web")}});
    };
    const QString id = actions->newProject(workGroup);
    QVERIFY(!id.isEmpty());
    const Project *project = model.project(id);
    QVERIFY(project);
    QCOMPARE(project->name, QStringLiteral("api"));
    QCOMPARE(project->groupId, workGroup);
    QCOMPARE(project->priority, Priority::Normal);
    QCOMPARE(project->order, 3);    // last in Work, after backend, frontend and infra
    QCOMPARE(project->folders.size(), size_t(1));
    QCOMPARE(settings->folderPath(QStringLiteral("web")), QStringLiteral("D:/web"));

    QVERIFY(actions->newProject(QStringLiteral("no such group")).isEmpty());
}

void TestEntityActions::newTask()
{
    fill = [](QDialog &dialog) {
        auto *task = qobject_cast<TaskDialog *>(&dialog);
        QVERIFY(task);
        task->setTitle(QStringLiteral("Write docs"));
    };
    const QString id = actions->newTask(backendProject);
    QVERIFY(!id.isEmpty());
    const Task *task = model.task(id);
    QVERIFY(task);
    QCOMPARE(task->title, QStringLiteral("Write docs"));
    QCOMPARE(task->projectId, backendProject);
    QCOMPARE(task->state, TaskState::Active);
    QCOMPARE(task->priority, Priority::Normal);
    QCOMPARE(task->order, 5);       // last in backend, after its five tasks (0 to 4)
    QCOMPARE(task->dashboards.size(), size_t(1));
    QCOMPARE(task->defaultDashboardId, task->dashboards.front().id);
}

void TestEntityActions::newItem()
{
    const size_t before = model.task(fixLoginTimeout)->items.size();
    fill = [](QDialog &dialog) {
        auto *item = qobject_cast<ItemDialog *>(&dialog);
        QVERIFY(item);
        item->setType(plainType);
        item->setName(QStringLiteral("Memo"));
    };
    const QString id = actions->newItem(fixLoginTimeout);
    QVERIFY(!id.isEmpty());
    const Item *item = model.item(fixLoginTimeout, id);
    QVERIFY(item);
    QCOMPARE(item->name, QStringLiteral("Memo"));
    QCOMPARE(item->type, plainType);
    QCOMPARE(item->config.value(QStringLiteral("note")).toString(), QStringLiteral("hello"));
    QCOMPARE(model.task(fixLoginTimeout)->items.size(), before + 1);

    // The editor gets the folders of the task's project.
    lastFolders.clear();
    fill = [](QDialog &dialog) {
        auto *item = qobject_cast<ItemDialog *>(&dialog);
        item->setType(editorType);
        item->setName(QStringLiteral("Own"));
    };
    QVERIFY(!actions->newItem(fixLoginTimeout).isEmpty());
    QCOMPARE(lastFolders.size(), size_t(2));
    QVERIFY(std::any_of(lastFolders.begin(), lastFolders.end(),
                        [](const auto &folder) { return folder.first == backendFolder; }));
    QVERIFY(std::any_of(lastFolders.begin(), lastFolders.end(),
                        [](const auto &folder) { return folder.first == deployFolder; }));
}

void TestEntityActions::newDashboard()
{
    const Task original = *model.task(fixLoginTimeout);
    const int preset = presets::names().size() - 1;
    fill = [preset](QDialog &dialog) {
        auto *dashboard = qobject_cast<DashboardDialog *>(&dialog);
        QVERIFY(dashboard);
        dashboard->setName(QStringLiteral("Wide"));
        dashboard->setPresetIndex(preset);
    };
    const QString id = actions->newDashboard(fixLoginTimeout);
    QVERIFY(!id.isEmpty());
    const Dashboard *dashboard = model.taskDashboard(fixLoginTimeout, id);
    QVERIFY(dashboard);
    QCOMPARE(dashboard->name, QStringLiteral("Wide"));

    QStringList ids;
    QStringList names;
    for (const Item &item : original.items) {
        ids << item.id;
        names << item.name;
    }
    QCOMPARE(layoutToJson(dashboard->layout), layoutToJson(presets::build(preset, ids, names)));
    QCOMPARE(model.task(fixLoginTimeout)->dashboards.size(), original.dashboards.size() + 1);
    QCOMPARE(model.task(fixLoginTimeout)->defaultDashboardId, original.defaultDashboardId);
}

void TestEntityActions::cancelledCreation()
{
    accept = false;
    const size_t groups = model.groups().size();
    const size_t projects = model.projectsOfGroup(workGroup).size();
    const size_t tasks = model.tasksOfProject(backendProject).size();
    const Task task = *model.task(fixLoginTimeout);

    QVERIFY(actions->newGroup().isEmpty());
    QVERIFY(actions->newProject(workGroup).isEmpty());
    QVERIFY(actions->newTask(backendProject).isEmpty());
    QVERIFY(actions->newItem(fixLoginTimeout).isEmpty());
    QVERIFY(actions->newDashboard(fixLoginTimeout).isEmpty());
    QCOMPARE(execCount, 5);

    QCOMPARE(model.groups().size(), groups);
    QCOMPARE(model.projectsOfGroup(workGroup).size(), projects);
    QCOMPARE(model.tasksOfProject(backendProject).size(), tasks);
    QCOMPARE(*model.task(fixLoginTimeout), task);

    // An edit cancelled changes nothing.
    const Group group = *model.group(workGroup);
    actions->editGroup(workGroup);
    QCOMPARE(*model.group(workGroup), group);

    // A dialog refusing its value is not applied either.
    accept = true;
    fill = [](QDialog &dialog) { qobject_cast<GroupDialog *>(&dialog)->setName(QString()); };
    QVERIFY(actions->newGroup().isEmpty());
    QCOMPARE(model.groups().size(), groups);
}

void TestEntityActions::editing()
{
    fill = [](QDialog &dialog) { qobject_cast<GroupDialog *>(&dialog)->setName(QStringLiteral("Office")); };
    actions->editGroup(workGroup);
    QCOMPARE(model.group(workGroup)->name, QStringLiteral("Office"));

    fill = [](QDialog &dialog) {
        auto *project = qobject_cast<ProjectDialog *>(&dialog);
        project->setName(QStringLiteral("api"));
        project->setGroupId(personalGroup);
        Folder folder = project->value().folders.front();
        project->setFolders({folder}, {{folder.id, QStringLiteral("C:/src/api")}});
    };
    actions->editProject(backendProject);
    QCOMPARE(model.project(backendProject)->name, QStringLiteral("api"));
    QCOMPARE(model.project(backendProject)->groupId, personalGroup);
    QCOMPARE(model.project(backendProject)->folders.size(), size_t(1));
    const QString folderId = model.project(backendProject)->folders.front().id;
    QCOMPARE(settings->folderPath(folderId), QStringLiteral("C:/src/api"));

    fill = [](QDialog &dialog) {
        auto *task = qobject_cast<TaskDialog *>(&dialog);
        task->setTitle(QStringLiteral("Fix it"));
        task->setState(TaskState::Done);
    };
    actions->editTask(fixLoginTimeout);
    QCOMPARE(model.task(fixLoginTimeout)->title, QStringLiteral("Fix it"));
    QCOMPARE(model.task(fixLoginTimeout)->state, TaskState::Done);

    const QString notes = itemId(model, fixLoginTimeout, notesItem);
    const Item original = *model.item(fixLoginTimeout, notes);
    fill = [](QDialog &dialog) { qobject_cast<ItemDialog *>(&dialog)->setName(QStringLiteral("Scratch")); };
    actions->editItem(fixLoginTimeout, notes);
    Item expected = original;
    expected.name = QStringLiteral("Scratch");
    QCOMPARE(*model.item(fixLoginTimeout, notes), expected);

    const QString review = dashboardId(model, fixLoginTimeout, fixLoginReviewDashboard);
    const LayoutNode layout = model.taskDashboard(fixLoginTimeout, review)->layout;
    fill = [](QDialog &dialog) { qobject_cast<DashboardDialog *>(&dialog)->setName(QStringLiteral("Code review")); };
    actions->renameDashboard(fixLoginTimeout, review);
    QCOMPARE(model.taskDashboard(fixLoginTimeout, review)->name, QStringLiteral("Code review"));
    QCOMPARE(layoutToJson(model.taskDashboard(fixLoginTimeout, review)->layout), layoutToJson(layout));
}

void TestEntityActions::removeProjectAndGroup()
{
    // Declined: nothing goes.
    answer = false;
    actions->removeProject(backendProject);
    QCOMPARE(questions, QStringList{QStringLiteral("Delete project backend and its 5 tasks?")});
    QVERIFY(model.project(backendProject));

    answer = true;
    actions->removeProject(backendProject);
    QVERIFY(!model.project(backendProject));
    QVERIFY(!model.task(fixLoginTimeout));

    questions.clear();
    actions->removeGroup(workGroup);
    QCOMPARE(questions, QStringList{QStringLiteral("Delete group Work, its 2 projects and 6 tasks?")});
    QVERIFY(!model.group(workGroup));
    QVERIFY(!model.project(frontendProject));

    // A project without tasks, and a group with one project.
    questions.clear();
    fill = [](QDialog &dialog) { qobject_cast<GroupDialog *>(&dialog)->setName(QStringLiteral("Solo")); };
    const QString solo = actions->newGroup();
    fill = [](QDialog &dialog) { qobject_cast<ProjectDialog *>(&dialog)->setName(QStringLiteral("empty")); };
    const QString empty = actions->newProject(solo);
    actions->removeProject(empty);
    actions->newProject(solo);
    actions->removeGroup(solo);
    QCOMPARE(questions, (QStringList{QStringLiteral("Delete project empty?"),
                                     QStringLiteral("Delete group Solo and its 1 project?")}));
    QVERIFY(!model.group(solo));
}

void TestEntityActions::removeTaskItemDashboard()
{
    const QString notes = itemId(model, fixLoginTimeout, notesItem);
    const size_t items = model.task(fixLoginTimeout)->items.size();

    answer = false;
    actions->removeItem(fixLoginTimeout, notes);
    QVERIFY(model.item(fixLoginTimeout, notes));

    answer = true;
    actions->removeItem(fixLoginTimeout, notes);
    QVERIFY(!model.item(fixLoginTimeout, notes));
    QCOMPARE(model.task(fixLoginTimeout)->items.size(), items - 1);

    const QString work = dashboardId(model, fixLoginTimeout, fixLoginWorkDashboard);
    const QString review = dashboardId(model, fixLoginTimeout, fixLoginReviewDashboard);
    QCOMPARE(model.task(fixLoginTimeout)->defaultDashboardId, work);
    actions->removeDashboard(fixLoginTimeout, work);
    QVERIFY(!model.taskDashboard(fixLoginTimeout, work));
    QCOMPARE(model.task(fixLoginTimeout)->defaultDashboardId, review);

    actions->removeTask(fixLoginTimeout);
    QVERIFY(!model.task(fixLoginTimeout));

    QCOMPARE(questions, (QStringList{QStringLiteral("Delete item Notes from task Fix login timeout?"),
                                     QStringLiteral("Delete item Notes from task Fix login timeout?"),
                                     QStringLiteral("Delete dashboard Work?"),
                                     QStringLiteral("Delete task Fix login timeout?")}));
}

void TestEntityActions::lastDashboard()
{
    const Task original = *model.task(upgradeGo);
    QCOMPARE(original.dashboards.size(), size_t(1));
    actions->removeDashboard(upgradeGo, original.dashboards.front().id);
    QVERIFY(questions.isEmpty());
    QCOMPARE(*model.task(upgradeGo), original);
}

void TestEntityActions::priorityAndState()
{
    // A new priority changes the marker only; the task keeps its place.
    const int order = model.task(backendOverview)->order;
    actions->setPriority(backendOverview, Priority::Urgent);
    QCOMPARE(model.task(backendOverview)->priority, Priority::Urgent);
    QCOMPARE(model.task(backendOverview)->order, order);

    actions->setPriority(frontendProject, Priority::Low);
    QCOMPARE(model.project(frontendProject)->priority, Priority::Low);

    actions->setState(backendOverview, TaskState::Archived);
    QCOMPARE(model.task(backendOverview)->state, TaskState::Archived);

    const QString review = dashboardId(model, fixLoginTimeout, fixLoginReviewDashboard);
    actions->setDefaultDashboard(fixLoginTimeout, review);
    QCOMPARE(model.task(fixLoginTimeout)->defaultDashboardId, review);
}

void TestEntityActions::moves()
{
    actions->moveGroup(personalGroup, 0);
    QCOMPARE(model.group(personalGroup)->order, 0);
    QCOMPARE(model.group(workGroup)->order, 1);

    // A move changes the order only, never the priority.
    const Priority taskPriority = model.task(backendOverview)->priority;
    actions->moveTask(backendOverview, 0);
    QCOMPARE(model.task(backendOverview)->priority, taskPriority);
    QCOMPARE(model.task(backendOverview)->order, 0);
    QCOMPARE(model.task(fixLoginTimeout)->order, 1);

    const Priority projectPriority = model.project(frontendProject)->priority;
    actions->moveProject(frontendProject, 0);
    QCOMPARE(model.project(frontendProject)->priority, projectPriority);
    QCOMPARE(model.project(frontendProject)->order, 0);
    QCOMPARE(model.project(backendProject)->order, 1);

    // Items and dashboards move within their task's lists.
    const std::vector<Item> items = model.task(fixLoginTimeout)->items;
    actions->moveItem(fixLoginTimeout, items.back().id, 0);
    QCOMPARE(model.task(fixLoginTimeout)->items.front().id, items.back().id);
    QCOMPARE(model.task(fixLoginTimeout)->items.size(), items.size());
    QCOMPARE(model.task(fixLoginTimeout)->items[1].id, items.front().id);

    const std::vector<Dashboard> dashboards = model.task(fixLoginTimeout)->dashboards;
    const QString defaultDashboard = model.task(fixLoginTimeout)->defaultDashboardId;
    actions->moveDashboard(fixLoginTimeout, dashboards.front().id, 1);
    QCOMPARE(model.task(fixLoginTimeout)->dashboards.back().id, dashboards.front().id);
    QCOMPARE(model.task(fixLoginTimeout)->defaultDashboardId, defaultDashboard);
}

void TestEntityActions::menus()
{
    QMenu group;
    actions->fillMenu(&group, SidebarModel::Kind::Group, workGroup, QString());
    QCOMPARE(texts(group), (QStringList{QStringLiteral("New project…"), QStringLiteral("New group…"),
                                        QStringLiteral("Edit group…"), QStringLiteral("Delete group…")}));

    QMenu project;
    actions->fillMenu(&project, SidebarModel::Kind::Project, backendProject, QString());
    QCOMPARE(texts(project), (QStringList{QStringLiteral("New task…"), QStringLiteral("Edit project…"),
                                          QStringLiteral("Priority"), QStringLiteral("Delete project…")}));
    QCOMPARE(checkedIn(project, QStringLiteral("Priority")), QStringLiteral("High"));

    QMenu task;
    actions->fillMenu(&task, SidebarModel::Kind::Task, fixLoginTimeout, fixLoginTimeout);
    QCOMPARE(texts(task), (QStringList{QStringLiteral("New item…"), QStringLiteral("Paste URL…"), QStringLiteral("Paste Image…"),
                                       QStringLiteral("New dashboard…"),
                                       QStringLiteral("Edit task…"), QStringLiteral("Priority"),
                                       QStringLiteral("State"), QStringLiteral("Delete task…")}));
    QCOMPARE(checkedIn(task, QStringLiteral("Priority")), QStringLiteral("Urgent"));
    QCOMPARE(checkedIn(task, QStringLiteral("State")), QStringLiteral("Active"));

    // Triggering a priority action applies it.
    const QAction *priority = actionNamed(task, QStringLiteral("Priority"));
    actionNamed(*priority->menu(), QStringLiteral("Low"))->trigger();
    QCOMPARE(model.task(fixLoginTimeout)->priority, Priority::Low);

    QMenu itemList;
    actions->fillMenu(&itemList, SidebarModel::Kind::ItemList, QString(), fixLoginTimeout);
    QCOMPARE(texts(itemList), (QStringList{QStringLiteral("New item…"), QStringLiteral("Paste URL…"), QStringLiteral("Paste Image…")}));

    QMenu dashboardList;
    actions->fillMenu(&dashboardList, SidebarModel::Kind::DashboardList, QString(), fixLoginTimeout);
    QCOMPARE(texts(dashboardList), QStringList{QStringLiteral("New dashboard…")});

    QMenu item;
    actions->fillMenu(&item, SidebarModel::Kind::Item, itemId(model, fixLoginTimeout, notesItem), fixLoginTimeout);
    QCOMPARE(texts(item), (QStringList{QStringLiteral("Edit item…"), QStringLiteral("Delete item…")}));

    // The default dashboard cannot be set as default again.
    const QString work = dashboardId(model, fixLoginTimeout, fixLoginWorkDashboard);
    const QString review = dashboardId(model, fixLoginTimeout, fixLoginReviewDashboard);
    QMenu defaultDashboard;
    actions->fillMenu(&defaultDashboard, SidebarModel::Kind::Dashboard, work, fixLoginTimeout);
    QCOMPARE(texts(defaultDashboard), (QStringList{QStringLiteral("Rename dashboard…"),
                                                   QStringLiteral("Set as default"),
                                                   QStringLiteral("Delete dashboard…")}));
    QVERIFY(!actionNamed(defaultDashboard, QStringLiteral("Set as default"))->isEnabled());
    QVERIFY(actionNamed(defaultDashboard, QStringLiteral("Delete dashboard…"))->isEnabled());

    QMenu other;
    actions->fillMenu(&other, SidebarModel::Kind::Dashboard, review, fixLoginTimeout);
    QVERIFY(actionNamed(other, QStringLiteral("Set as default"))->isEnabled());
    actionNamed(other, QStringLiteral("Set as default"))->trigger();
    QCOMPARE(model.task(fixLoginTimeout)->defaultDashboardId, review);

    // The last dashboard cannot be deleted.
    QMenu last;
    actions->fillMenu(&last, SidebarModel::Kind::Dashboard, model.task(upgradeGo)->dashboards.front().id, upgradeGo);
    QVERIFY(!actionNamed(last, QStringLiteral("Delete dashboard…"))->isEnabled());

    // An unknown id adds nothing.
    QMenu unknown;
    actions->fillMenu(&unknown, SidebarModel::Kind::Task, QStringLiteral("no such task"), QString());
    QVERIFY(unknown.actions().isEmpty());
}


// Moving to the trash sets trashedAt without a question; restoring clears it.
// A project takes its tasks with it, without changing them.
void TestEntityActions::trashAndRestore()
{
    actions->trashTask(upgradeGo);
    QVERIFY(model.task(upgradeGo));
    const QDateTime when = QDateTime::fromString(model.task(upgradeGo)->trashedAt, Qt::ISODate);
    QVERIFY(when.isValid());
    QCOMPARE(when.timeSpec(), Qt::UTC);
    QVERIFY(model.taskInTrash(upgradeGo));

    actions->trashProject(backendProject);
    QVERIFY(model.project(backendProject)->isTrashed());
    QVERIFY(model.taskInTrash(fixLoginTimeout));
    QVERIFY(!model.task(fixLoginTimeout)->isTrashed());
    QVERIFY(questions.isEmpty());
    QCOMPARE(actions->trashedProjectCount(), 1);
    QCOMPARE(actions->trashedTaskCount(), 5);

    actions->restoreProject(backendProject);
    QVERIFY(!model.project(backendProject)->isTrashed());
    QVERIFY(!model.taskInTrash(fixLoginTimeout));
    // The task in the trash on its own stays there.
    QVERIFY(model.taskInTrash(upgradeGo));
    actions->restoreTask(upgradeGo);
    QVERIFY(!model.taskInTrash(upgradeGo));
    QVERIFY(model.task(upgradeGo)->trashedAt.isEmpty());
}

void TestEntityActions::restoreTaskRestoresProject()
{
    actions->trashTask(fixLoginTimeout);
    actions->trashProject(backendProject);
    actions->restoreTask(fixLoginTimeout);
    QVERIFY(!model.project(backendProject)->isTrashed());
    QVERIFY(!model.task(fixLoginTimeout)->isTrashed());

    // A task that is not in the trash itself restores its project.
    actions->trashProject(backendProject);
    actions->restoreTask(backendOverview);
    QVERIFY(!model.project(backendProject)->isTrashed());
}

// "Delete project…" and "Delete task…" move to the trash without a question.
void TestEntityActions::deleteMenuMovesToTrash()
{
    QMenu task;
    actions->fillMenu(&task, SidebarModel::Kind::Task, fixLoginTimeout, fixLoginTimeout);
    actionNamed(task, QStringLiteral("Delete task…"))->trigger();
    QVERIFY(model.task(fixLoginTimeout));
    QVERIFY(model.task(fixLoginTimeout)->isTrashed());

    QMenu project;
    actions->fillMenu(&project, SidebarModel::Kind::Project, frontendProject, QString());
    actionNamed(project, QStringLiteral("Delete project…"))->trigger();
    QVERIFY(model.project(frontendProject));
    QVERIFY(model.project(frontendProject)->isTrashed());
    QVERIFY(questions.isEmpty());
}

// With Shift held, they delete for good after the usual question.
void TestEntityActions::shiftDeletesPermanently()
{
    modifiers = Qt::ShiftModifier;
    QMenu task;
    actions->fillMenu(&task, SidebarModel::Kind::Task, fixLoginTimeout, fixLoginTimeout);
    answer = false;
    actionNamed(task, QStringLiteral("Delete task…"))->trigger();
    QVERIFY(model.task(fixLoginTimeout));
    QVERIFY(!model.task(fixLoginTimeout)->isTrashed());
    answer = true;
    actionNamed(task, QStringLiteral("Delete task…"))->trigger();
    QVERIFY(!model.task(fixLoginTimeout));

    QMenu project;
    actions->fillMenu(&project, SidebarModel::Kind::Project, backendProject, QString());
    actionNamed(project, QStringLiteral("Delete project…"))->trigger();
    QVERIFY(!model.project(backendProject));
    QCOMPARE(questions, (QStringList{QStringLiteral("Delete task Fix login timeout?"),
                                     QStringLiteral("Delete task Fix login timeout?"),
                                     QStringLiteral("Delete project backend and its 4 tasks?")}));
}

// One question names the counts; declining keeps everything.
void TestEntityActions::emptyTrash()
{
    // An empty trash asks nothing.
    actions->emptyTrash();
    QVERIFY(questions.isEmpty());

    actions->trashProject(backendProject);
    const QString frontendTask = model.tasksOfProject(frontendProject).front()->id;
    actions->trashTask(frontendTask);

    answer = false;
    actions->emptyTrash();
    QCOMPARE(questions, QStringList{QStringLiteral("Permanently delete 1 project and 6 tasks in the trash?")});
    QVERIFY(model.project(backendProject));

    answer = true;
    actions->emptyTrash();
    QVERIFY(!model.project(backendProject));
    QVERIFY(!model.task(fixLoginTimeout));
    QVERIFY(!model.task(frontendTask));
    QVERIFY(model.project(frontendProject));
    QCOMPARE(actions->trashedProjectCount(), 0);
    QCOMPARE(actions->trashedTaskCount(), 0);
}

// The Trash list's menu: Restore and Delete permanently, on projects and tasks.
void TestEntityActions::trashMenus()
{
    actions->trashProject(backendProject);
    QMenu project;
    actions->fillTrashMenu(&project, SidebarModel::Kind::Project, backendProject, QString());
    QCOMPARE(texts(project), (QStringList{QStringLiteral("Restore"), QStringLiteral("Delete permanently…")}));
    actionNamed(project, QStringLiteral("Restore"))->trigger();
    QVERIFY(!model.project(backendProject)->isTrashed());

    actions->trashTask(fixLoginTimeout);
    QMenu task;
    actions->fillTrashMenu(&task, SidebarModel::Kind::Task, fixLoginTimeout, fixLoginTimeout);
    QCOMPARE(texts(task), (QStringList{QStringLiteral("Restore"), QStringLiteral("Delete permanently…")}));
    actionNamed(task, QStringLiteral("Delete permanently…"))->trigger();
    QCOMPARE(questions, QStringList{QStringLiteral("Delete task Fix login timeout?")});
    QVERIFY(!model.task(fixLoginTimeout));

    // Groups, items and dashboards get nothing.
    QMenu group;
    actions->fillTrashMenu(&group, SidebarModel::Kind::Group, workGroup, QString());
    QVERIFY(group.actions().isEmpty());
}

// "Paste Image…" on a task and its Items row is enabled only while the
// clipboard holds an image. A paste that cannot happen, here without an
// image type, says why in a message and opens no dialog. The paste itself,
// and the read-only sample data, are tested in tst_main_window.
void TestEntityActions::pasteImageMenu()
{
    const auto pasteAction = [this](SidebarModel::Kind kind, const QString &id, QMenu &menu) {
        actions->fillMenu(&menu, kind, id, fixLoginTimeout);
        return actionNamed(menu, QStringLiteral("Paste Image…"));
    };
    QMenu taskMenu;
    QMenu itemListMenu;
    QAction *onTask = pasteAction(SidebarModel::Kind::Task, fixLoginTimeout, taskMenu);
    QAction *onItems = pasteAction(SidebarModel::Kind::ItemList, QString(), itemListMenu);
    QVERIFY(onTask && onItems);
    QVERIFY(!onTask->isEnabled());
    QVERIFY(!onItems->isEnabled());

    clipboard = QImage(4, 3, QImage::Format_ARGB32);
    clipboard.fill(Qt::red);
    QMenu taskMenu2;
    QMenu itemListMenu2;
    onTask = pasteAction(SidebarModel::Kind::Task, fixLoginTimeout, taskMenu2);
    onItems = pasteAction(SidebarModel::Kind::ItemList, QString(), itemListMenu2);
    QVERIFY(onTask->isEnabled());
    QVERIFY(onItems->isEnabled());

    QSignalSpy messages(actions.get(), &EntityActions::message);
    const Task before = *model.task(fixLoginTimeout);
    onItems->trigger();
    QCOMPARE(execCount, 0);
    QCOMPARE(messages.size(), 1);
    QCOMPARE(messages.at(0).at(0).toString(), ImagePaste::message(ImagePaste::Result::NoImageType));
    QVERIFY(*model.task(fixLoginTimeout) == before);
}

// A token typed into a settings screen goes to the secret store under
// "<workspace id>/<profile id>/<module id>/<field key>", never into the
// workspace. Without a secret store it is dropped.
void TestEntityActions::typedTokenIsWritten()
{
    QCOMPARE(model.workspaceId(), fixtureWorkspaceId);
    SecretStore secrets(makeMemoryBackend());
    fill = [](QDialog &dialog) {
        if (QLineEdit *line = tokenLine(dialog))
            line->setText(QStringLiteral(" s3cret "));
    };

    // Dropped without a store.
    actions->editModuleSettings(workProfile, QStringLiteral("tokens"), QStringLiteral("general"));
    QCOMPARE(execCount, 1);
    QVERIFY(!model.moduleSettings(workProfile, QStringLiteral("tokens"), QStringLiteral("general"))
                 .contains(QStringLiteral("token")));

    actions->setSecretStore(&secrets);
    QSignalSpy changed(&secrets, &SecretStore::changed);
    actions->editModuleSettings(workProfile, QStringLiteral("tokens"), QStringLiteral("general"));
    QCOMPARE(changed.count(), 1);
    QCOMPARE(changed.at(0).at(0).toString(), tokenKey(workProfile));
    QCOMPARE(secrets.cached(tokenKey(workProfile)), std::optional<QString>(QStringLiteral("s3cret")));
    QVERIFY(!model.moduleSettings(workProfile, QStringLiteral("tokens"), QStringLiteral("general"))
                 .contains(QStringLiteral("token")));

    // Typing nothing keeps it.
    fill = nullptr;
    actions->editModuleSettings(workProfile, QStringLiteral("tokens"), QStringLiteral("general"));
    QCOMPARE(changed.count(), 1);
    QCOMPARE(secrets.cached(tokenKey(workProfile)), std::optional<QString>(QStringLiteral("s3cret")));
}

// The screen shows whether the token is set, read from the store, and its
// Remove button removes it on OK.
void TestEntityActions::tokenStateAndRemove()
{
    SecretStore secrets(makeMemoryBackend());
    actions->setSecretStore(&secrets);
    secrets.write(tokenKey(defaultProfile), QStringLiteral("old"));

    // Not known yet in this session: the form shows the state once read.
    QString notSet;
    fill = [&notSet](QDialog &dialog) {
        QLineEdit *line = tokenLine(dialog);
        QVERIFY(line);
        QTRY_COMPARE(line->placeholderText(), QStringLiteral("Not set on this device."));
        notSet = line->placeholderText();
    };
    actions->editModuleSettings(workProfile, QStringLiteral("tokens"), QStringLiteral("general"));
    QCOMPARE(notSet, QStringLiteral("Not set on this device."));
    QCOMPARE(secrets.state(tokenKey(workProfile)), SecretStore::State::NotSet);

    // A set token is never shown, and Remove removes it.
    QString shown;
    fill = [&shown](QDialog &dialog) {
        QLineEdit *line = tokenLine(dialog);
        QVERIFY(line);
        shown = line->text() + line->placeholderText();
        dialog.findChild<QPushButton *>(QStringLiteral("removeSecret_token"))->click();
    };
    actions->editModuleSettings(defaultProfile, QStringLiteral("tokens"), QStringLiteral("general"));
    QCOMPARE(shown, QStringLiteral("Set on this device. Type to replace it."));
    QCOMPARE(secrets.state(tokenKey(defaultProfile)), SecretStore::State::NotSet);

    // A cancelled dialog removes nothing.
    secrets.write(tokenKey(defaultProfile), QStringLiteral("again"));
    accept = false;
    actions->editModuleSettings(defaultProfile, QStringLiteral("tokens"), QStringLiteral("general"));
    QCOMPARE(secrets.state(tokenKey(defaultProfile)), SecretStore::State::Set);
}

// Deleting a profile removes its tokens on this device, and only its own.
void TestEntityActions::deletingProfileRemovesTokens()
{
    SecretStore secrets(makeMemoryBackend());
    actions->setSecretStore(&secrets);
    secrets.write(tokenKey(workProfile), QStringLiteral("work"));
    secrets.write(tokenKey(defaultProfile), QStringLiteral("default"));
    actions->setEditProfiles([](ProfilesDialog &dialog) {
        dialog.select(workProfile);
        dialog.deleteSelected();
    });
    actions->editProfiles();
    QVERIFY(!model.profile(workProfile));
    QCOMPARE(secrets.state(tokenKey(workProfile)), SecretStore::State::NotSet);
    QCOMPARE(secrets.state(tokenKey(defaultProfile)), SecretStore::State::Set);
}

QTEST_MAIN(TestEntityActions)
#include "tst_entity_actions.moc"
