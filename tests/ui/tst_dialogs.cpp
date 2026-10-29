// Tests the group, project, task, item and dashboard dialogs: values read back,
// refusal of an empty name, the group's connections and shared sites, the one
// main folder, and the item editor paths: a form from fields, a type's own
// editor, and an item of a missing module.
#include "core/DeviceSettings.h"
#include "core/Model.h"
#include "core/Store.h"
#include "items/ItemTypeRegistry.h"
#include "layout/Presets.h"
#include "ui/DashboardDialog.h"
#include "ui/FieldForm.h"
#include "ui/GroupDialog.h"
#include "ui/ItemDialog.h"
#include "ui/ProjectDialog.h"
#include "ui/TaskDialog.h"

#include <mirante/ItemEditor.h>
#include <mirante/Module.h>

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QStandardItemModel>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTest>

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

const QString formType = QStringLiteral("testmod.form");
const QString editorType = QStringLiteral("testmod.editor");

// An editor of its own: one line edit for "text". "bad" is a problem.
class TestEditor : public ItemEditor
{
public:
    explicit TestEditor(QWidget *parent)
        : ItemEditor(parent)
        , line(new QLineEdit(this))
    {
        line->setObjectName(QStringLiteral("text"));
    }

    void setConfig(const QJsonObject &config) override
    {
        given = config;
        line->setText(config.value(QStringLiteral("text")).toString());
    }

    QJsonObject config() const override
    {
        QJsonObject config = given;
        config.insert(QStringLiteral("text"), line->text());
        return config;
    }

    QStringList problems() const override
    {
        return line->text() == QStringLiteral("bad") ? QStringList{QStringLiteral("Text is bad")} : QStringList{};
    }

    QLineEdit *line;
    QJsonObject given;
};

// A type edited by a form: a required path and a URL with a default.
class FormType : public ItemType
{
public:
    QString id() const override { return formType; }
    QString name() const override { return QStringLiteral("Form type"); }
    QIcon icon() const override { return {}; }

    std::vector<Field> fields() const override
    {
        Field path;
        path.key = QStringLiteral("path");
        path.label = QStringLiteral("Path");
        path.required = true;
        Field url;
        url.key = QStringLiteral("url");
        url.label = QStringLiteral("Address");
        url.type = Field::Type::Url;
        url.defaultValue = QStringLiteral("https://example.com");
        return {path, url};
    }
};

// A type with its own editor.
class EditorType : public ItemType
{
public:
    QString id() const override { return editorType; }
    QString name() const override { return QStringLiteral("Own editor"); }
    QIcon icon() const override { return {}; }

    ItemEditor *createEditor(const EditorContext &, QWidget *parent) const override
    {
        return new TestEditor(parent);
    }
};

void registerTestModule(ModuleRegistrar &registrar)
{
    registrar.addItemType(std::make_unique<FormType>());
    registrar.addItemType(std::make_unique<EditorType>());
}

QTableWidgetItem *mainCheck(ProjectDialog &dialog, int row)
{
    auto *table = dialog.findChild<QTableWidget *>(QStringLiteral("folders"));
    return table ? table->item(row, 2) : nullptr;
}

} // namespace

class TestDialogs : public QObject
{
    Q_OBJECT

    Model model;
    ItemTypeRegistry types;
    std::unique_ptr<QTemporaryDir> dir;
    std::unique_ptr<QSettings> settings;

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void group();
    void groupConnectionsWithoutKinds();
    void groupSharedSites();
    void project();
    void projectFolders();
    void projectMainFolder();
    void projectNewFolder();
    void task();
    void itemWithForm();
    void itemWithOwnEditor();
    void itemChangingType();
    void itemOfMissingModule();
    void dashboard();
};

void TestDialogs::initTestCase()
{
    store::load(QStringLiteral(MIRANTE_FIXTURES_DIR), model);
    QVERIFY(model.project(backendProject));
    types.registerModule(QStringLiteral("testmod"), registerTestModule);
}

void TestDialogs::init()
{
    dir = std::make_unique<QTemporaryDir>();
    QVERIFY(dir->isValid());
    settings = std::make_unique<QSettings>(dir->filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
}

void TestDialogs::cleanup()
{
    settings.reset();
    dir.reset();
}

void TestDialogs::group()
{
    const Group original = *model.group(workGroup);
    GroupDialog dialog(original);
    QCOMPARE(dialog.value(), original);

    dialog.setName(QStringLiteral("   "));
    dialog.accept();
    QCOMPARE(dialog.result(), int(QDialog::Rejected));
    QVERIFY(!dialog.findChild<QLabel *>(QStringLiteral("message"))->text().isEmpty());

    dialog.setName(QStringLiteral(" Office "));
    dialog.accept();
    QCOMPARE(dialog.result(), int(QDialog::Accepted));
    Group expected = original;
    expected.name = QStringLiteral("Office");
    QCOMPARE(dialog.value(), expected);
}

void TestDialogs::groupConnectionsWithoutKinds()
{
    // No kinds are offered, so every connection is shown read-only and kept.
    const Group original = *model.group(workGroup);
    GroupDialog dialog(original);
    auto *table = dialog.findChild<QTableWidget *>(QStringLiteral("connections"));
    QCOMPARE(table->rowCount(), 5);
    QCOMPARE(table->item(4, 0)->text(), QStringLiteral("timetrack"));
    QVERIFY(!(table->item(0, 1)->flags() & Qt::ItemIsEditable));
    QVERIFY(!(table->item(0, 3)->flags() & Qt::ItemIsEnabled));
    QCOMPARE(table->item(0, 3)->checkState(), Qt::Checked);   // the gitlab default
    QCOMPARE(table->item(1, 3)->checkState(), Qt::Unchecked);
    QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("addConnection"))->isEnabled());
    auto *remove = dialog.findChild<QPushButton *>(QStringLiteral("removeConnection"));
    table->setCurrentCell(1, 1);
    QVERIFY(!remove->isEnabled());
    remove->click();

    // Their fields and extra survive, and OK is not refused for them.
    dialog.accept();
    QCOMPARE(dialog.result(), int(QDialog::Accepted));
    QCOMPARE(dialog.value(), original);
}

void TestDialogs::groupSharedSites()
{
    Group original = *model.group(workGroup);
    original.profileOverrides.insert(QStringLiteral("own.example"), QStringLiteral("group"));
    GroupDialog dialog(original);
    QCOMPARE(dialog.value(), original);

    auto *list = dialog.findChild<QListWidget *>(QStringLiteral("sharedSites"));
    QCOMPARE(list->count(), 2);   // the override that is not "shared" is not listed
    QCOMPARE(list->item(0)->text(), QStringLiteral("figma.com"));
    QCOMPARE(list->item(1)->text(), QStringLiteral("www.w3.org"));

    // Remove one, add two: a host in capitals and a pasted URL. A blank one
    // and a repeat are dropped.
    list->setCurrentRow(0);
    dialog.findChild<QPushButton *>(QStringLiteral("removeSite"))->click();
    auto *add = dialog.findChild<QPushButton *>(QStringLiteral("addSite"));
    add->click();
    list->item(list->count() - 1)->setText(QStringLiteral(" Acme.Atlassian.net "));
    add->click();
    list->item(list->count() - 1)->setText(QStringLiteral("https://gitlab.com/acme/api/-/merge_requests"));
    add->click();
    add->click();
    list->item(list->count() - 1)->setText(QStringLiteral("www.w3.org"));

    // The other override is kept as it is.
    Group expected = original;
    expected.profileOverrides.remove(QStringLiteral("figma.com"));
    expected.profileOverrides.insert(QStringLiteral("acme.atlassian.net"), QStringLiteral("shared"));
    expected.profileOverrides.insert(QStringLiteral("gitlab.com"), QStringLiteral("shared"));
    QCOMPARE(dialog.value().profileOverrides, expected.profileOverrides);
    QCOMPARE(dialog.value(), expected);
}

void TestDialogs::project()
{
    const Project original = *model.project(backendProject);
    const DeviceSettings device(settings.get());
    ProjectDialog dialog(original, model, device);
    QCOMPARE(dialog.value(), original);

    // Groups in manual order.
    auto *groups = dialog.findChild<QComboBox *>(QStringLiteral("group"));
    QCOMPARE(groups->count(), 2);
    QCOMPARE(groups->itemData(0).toString(), workGroup);
    QCOMPARE(groups->itemData(1).toString(), personalGroup);

    dialog.setName(QString());
    dialog.accept();
    QCOMPARE(dialog.result(), int(QDialog::Rejected));

    dialog.setName(QStringLiteral("api"));
    dialog.setGroupId(personalGroup);
    dialog.setPriority(Priority::Low);
    dialog.accept();
    QCOMPARE(dialog.result(), int(QDialog::Accepted));

    Project expected = original;
    expected.name = QStringLiteral("api");
    expected.groupId = personalGroup;
    expected.priority = Priority::Low;
    QCOMPARE(dialog.value(), expected);
}

void TestDialogs::projectFolders()
{
    DeviceSettings device(settings.get());
    device.setFolderPath(backendFolder, QStringLiteral("C:/src/backend"));
    ProjectDialog dialog(*model.project(backendProject), model, device);

    // Paths come from the settings, and are not written back.
    QMap<QString, QString> paths = dialog.folderPaths();
    QCOMPARE(paths.size(), 2);
    QCOMPARE(paths.value(backendFolder), QStringLiteral("C:/src/backend"));
    QCOMPARE(paths.value(deployFolder), QString());

    // A folder without a path says so.
    QStringList labels;
    for (const QLabel *label : dialog.findChildren<QLabel *>(QStringLiteral("path")))
        labels << label->text();
    QVERIFY(labels.contains(QStringLiteral("Folder not set on this device")));

    // Replace the folders.
    Folder web;
    web.id = QStringLiteral("web");
    web.name = QStringLiteral("web");
    web.remoteUrl = QStringLiteral("https://example.com/web.git");
    web.main = true;
    dialog.setFolders({web}, {{QStringLiteral("web"), QStringLiteral("D:/web")}});
    QCOMPARE(dialog.value().folders.size(), size_t(1));
    QCOMPARE(dialog.value().folders.front(), web);
    QCOMPARE(dialog.folderPaths(), (QMap<QString, QString>{{QStringLiteral("web"), QStringLiteral("D:/web")}}));
    QCOMPARE(device.folderPath(QStringLiteral("web")), QString());

    // Edits in the table reach the value.
    auto *table = dialog.findChild<QTableWidget *>(QStringLiteral("folders"));
    table->item(0, 0)->setText(QStringLiteral("website"));
    table->item(0, 1)->setText(QStringLiteral("git@example.com:web.git"));
    QCOMPARE(dialog.value().folders.front().name, QStringLiteral("website"));
    QCOMPARE(dialog.value().folders.front().remoteUrl, QStringLiteral("git@example.com:web.git"));
}

void TestDialogs::projectMainFolder()
{
    const DeviceSettings device(settings.get());
    ProjectDialog dialog(*model.project(backendProject), model, device);

    Folder a;
    a.id = QStringLiteral("a");
    a.name = QStringLiteral("a");
    Folder b = a;
    b.id = QStringLiteral("b");
    b.name = QStringLiteral("b");

    // None main: the first becomes main.
    dialog.setFolders({a, b}, {});
    QVERIFY(dialog.value().folders[0].main);
    QVERIFY(!dialog.value().folders[1].main);

    // Two main: only the first stays.
    a.main = true;
    b.main = true;
    dialog.setFolders({a, b}, {});
    QVERIFY(dialog.value().folders[0].main);
    QVERIFY(!dialog.value().folders[1].main);

    // Checking another folder moves main to it.
    mainCheck(dialog, 1)->setCheckState(Qt::Checked);
    QVERIFY(!dialog.value().folders[0].main);
    QVERIFY(dialog.value().folders[1].main);
    QCOMPARE(mainCheck(dialog, 0)->checkState(), Qt::Unchecked);

    // Unchecking the main folder is undone.
    mainCheck(dialog, 1)->setCheckState(Qt::Unchecked);
    QVERIFY(dialog.value().folders[1].main);
    QCOMPARE(mainCheck(dialog, 1)->checkState(), Qt::Checked);

    // Removing the main folder makes the remaining one main.
    auto *table = dialog.findChild<QTableWidget *>(QStringLiteral("folders"));
    table->setCurrentCell(1, 0);
    dialog.findChild<QPushButton *>(QStringLiteral("removeFolder"))->click();
    QCOMPARE(dialog.value().folders.size(), size_t(1));
    QCOMPARE(dialog.value().folders[0].id, QStringLiteral("a"));
    QVERIFY(dialog.value().folders[0].main);
}

void TestDialogs::projectNewFolder()
{
    const DeviceSettings device(settings.get());
    Project project;
    project.id = QStringLiteral("new");
    project.groupId = workGroup;
    ProjectDialog dialog(project, model, device);
    dialog.setName(QStringLiteral("new project"));

    auto *add = dialog.findChild<QPushButton *>(QStringLiteral("addFolder"));
    add->click();
    add->click();
    const std::vector<Folder> folders = dialog.value().folders;
    QCOMPARE(folders.size(), size_t(2));
    QVERIFY(!folders[0].id.isEmpty());
    QVERIFY(folders[0].id != folders[1].id);
    QVERIFY(folders[0].main);   // the first new folder is main
    QVERIFY(!folders[1].main);
    QCOMPARE(dialog.folderPaths().value(folders[0].id), QString());

    // Folders need names.
    dialog.accept();
    QCOMPARE(dialog.result(), int(QDialog::Rejected));
    auto *table = dialog.findChild<QTableWidget *>(QStringLiteral("folders"));
    table->item(0, 0)->setText(QStringLiteral("one"));
    table->item(1, 0)->setText(QStringLiteral("two"));
    dialog.accept();
    QCOMPARE(dialog.result(), int(QDialog::Accepted));
}

void TestDialogs::task()
{
    const Task original = *model.task(fixLoginTimeout);
    TaskDialog dialog(original, model);
    QCOMPARE(dialog.value(), original);

    // Group headings cannot be chosen; every project is listed.
    auto *projects = dialog.findChild<QComboBox *>(QStringLiteral("project"));
    auto *items = qobject_cast<QStandardItemModel *>(projects->model());
    QVERIFY(items);
    QCOMPARE(projects->itemText(0), QStringLiteral("Work"));
    QCOMPARE(items->item(0)->flags() & Qt::ItemIsEnabled, Qt::ItemFlags());
    int projectCount = 0;
    for (int i = 0; i < projects->count(); ++i)
        projectCount += projects->itemData(i).isValid() ? 1 : 0;
    QCOMPARE(projectCount, 5);

    dialog.setTitle(QString());
    dialog.accept();
    QCOMPARE(dialog.result(), int(QDialog::Rejected));

    dialog.setTitle(QStringLiteral("Fix it"));
    dialog.setProjectId(frontendProject);
    dialog.setPriority(Priority::Low);
    dialog.setState(TaskState::Done);
    dialog.accept();
    QCOMPARE(dialog.result(), int(QDialog::Accepted));

    Task expected = original;
    expected.title = QStringLiteral("Fix it");
    expected.projectId = frontendProject;
    expected.priority = Priority::Low;
    expected.state = TaskState::Done;
    QCOMPARE(dialog.value(), expected);
}

void TestDialogs::itemWithForm()
{
    Item item;
    item.id = QStringLiteral("i1");
    item.type = formType;
    item.name = QStringLiteral("Paths");
    item.config = {{QStringLiteral("path"), QStringLiteral("a/b")}, {QStringLiteral("other"), 7}};
    ItemDialog dialog(item, false, types, {});
    QVERIFY(qobject_cast<FieldForm *>(dialog.editor()));
    QCOMPARE(dialog.value(), item);
    QVERIFY(!dialog.findChild<QComboBox *>(QStringLiteral("type")));

    dialog.setName(QString());
    dialog.accept();
    QCOMPARE(dialog.result(), int(QDialog::Rejected));

    // A problem in the form refuses OK.
    dialog.setName(QStringLiteral("Renamed"));
    auto *form = qobject_cast<FieldForm *>(dialog.editor());
    auto *path = qobject_cast<QLineEdit *>(form->editorFor(QStringLiteral("path")));
    QVERIFY(path);
    path->selectAll();
    QTest::keyClick(path, Qt::Key_Delete);
    QVERIFY(!form->problems().isEmpty());
    dialog.accept();
    QCOMPARE(dialog.result(), int(QDialog::Rejected));
    QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("message"))->text().contains(form->problems().front()));

    QTest::keyClicks(path, QStringLiteral("c/d"));
    dialog.accept();
    QCOMPARE(dialog.result(), int(QDialog::Accepted));
    const Item value = dialog.value();
    QCOMPARE(value.name, QStringLiteral("Renamed"));
    QCOMPARE(value.config.value(QStringLiteral("path")).toString(), QStringLiteral("c/d"));
    QCOMPARE(value.config.value(QStringLiteral("other")).toInt(), 7);
}

void TestDialogs::itemWithOwnEditor()
{
    Item item;
    item.id = QStringLiteral("i2");
    item.type = editorType;
    item.name = QStringLiteral("Own");
    item.config = {{QStringLiteral("text"), QStringLiteral("hello")}, {QStringLiteral("kept"), true}};
    ItemDialog dialog(item, false, types, {});
    auto *editor = dynamic_cast<TestEditor *>(dialog.editor());
    QVERIFY(editor);
    QCOMPARE(dialog.value(), item);

    editor->line->setText(QStringLiteral("bad"));
    dialog.accept();
    QCOMPARE(dialog.result(), int(QDialog::Rejected));
    QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("message"))->text().contains(QStringLiteral("Text is bad")));

    editor->line->setText(QStringLiteral("good"));
    dialog.accept();
    QCOMPARE(dialog.result(), int(QDialog::Accepted));
    Item expected = item;
    expected.config.insert(QStringLiteral("text"), QStringLiteral("good"));
    QCOMPARE(dialog.value(), expected);
}

void TestDialogs::itemChangingType()
{
    // A new item with no type gets the first type by name: "Form type".
    Item item;
    item.id = QStringLiteral("i3");
    ItemDialog dialog(item, true, types, {});
    auto *combo = dialog.findChild<QComboBox *>(QStringLiteral("type"));
    QVERIFY(combo);
    QCOMPARE(combo->count(), 2);
    QCOMPARE(dialog.value().type, formType);
    QVERIFY(qobject_cast<FieldForm *>(dialog.editor()));
    QCOMPARE(dialog.value().config,
             (QJsonObject{{QStringLiteral("url"), QStringLiteral("https://example.com")}}));

    dialog.setName(QStringLiteral("New"));
    dialog.setType(editorType);
    QCOMPARE(dialog.value().type, editorType);
    QVERIFY(dynamic_cast<TestEditor *>(dialog.editor()));
    QCOMPARE(dialog.value().config, (QJsonObject{{QStringLiteral("text"), QString()}}));
    QCOMPARE(dialog.findChildren<QWidget *>(QStringLiteral("editor")).size(), 1);

    dialog.setType(formType);
    QVERIFY(qobject_cast<FieldForm *>(dialog.editor()));
    QCOMPARE(dialog.value().config,
             (QJsonObject{{QStringLiteral("url"), QStringLiteral("https://example.com")}}));
    QCOMPARE(dialog.value().name, QStringLiteral("New"));
}

void TestDialogs::itemOfMissingModule()
{
    Item item;
    item.id = QStringLiteral("i4");
    item.type = QStringLiteral("gone.thing");
    item.name = QStringLiteral("Old");
    item.config = {{QStringLiteral("anything"), QStringLiteral("kept")}};
    ItemDialog dialog(item, false, types, {});
    QVERIFY(!dialog.editor());
    QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("missing")));
    QCOMPARE(dialog.value(), item);

    dialog.setName(QString());
    dialog.accept();
    QCOMPARE(dialog.result(), int(QDialog::Rejected));

    dialog.setName(QStringLiteral("Renamed"));
    dialog.accept();
    QCOMPARE(dialog.result(), int(QDialog::Accepted));
    Item expected = item;
    expected.name = QStringLiteral("Renamed");
    QCOMPARE(dialog.value(), expected);
}

void TestDialogs::dashboard()
{
    Dashboard original;
    original.id = QStringLiteral("d1");
    original.name = QStringLiteral("Main");

    DashboardDialog renaming(original, false);
    QCOMPARE(renaming.value(), original);
    QCOMPARE(renaming.presetIndex(), -1);
    QVERIFY(!renaming.findChild<QComboBox *>(QStringLiteral("preset")));
    renaming.setName(QString());
    renaming.accept();
    QCOMPARE(renaming.result(), int(QDialog::Rejected));
    renaming.setName(QStringLiteral("Review"));
    renaming.accept();
    QCOMPARE(renaming.result(), int(QDialog::Accepted));
    QCOMPARE(renaming.value().name, QStringLiteral("Review"));

    Dashboard fresh;
    fresh.id = QStringLiteral("d2");
    DashboardDialog creating(fresh, true);
    QCOMPARE(creating.presetIndex(), 0);
    auto *presetCombo = creating.findChild<QComboBox *>(QStringLiteral("preset"));
    QVERIFY(presetCombo);
    QCOMPARE(presetCombo->count(), presets::names().size());
    creating.setPresetIndex(2);
    QCOMPARE(creating.presetIndex(), 2);
    creating.accept();
    QCOMPARE(creating.result(), int(QDialog::Rejected));
    creating.setName(QStringLiteral("Overview"));
    creating.accept();
    QCOMPARE(creating.result(), int(QDialog::Accepted));
    Dashboard expected = fresh;
    expected.name = QStringLiteral("Overview");
    QCOMPARE(creating.value(), expected);
}

QTEST_MAIN(TestDialogs)
#include "tst_dialogs.moc"
