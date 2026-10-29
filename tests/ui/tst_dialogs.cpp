// Tests the group, project, task, item, dashboard and web profiles dialogs:
// values read back, refusal of an empty name, the group's connections and web
// profile, the one main folder, the item editor paths (a form from fields, a
// type's own editor, an item of a missing module), the type grid of a new
// item, the item's web profile, and
// creating, renaming and deleting web profiles.
#include "core/DeviceSettings.h"
#include "core/GroupColours.h"
#include "core/Model.h"
#include "core/Store.h"
#include "core/WebProfiles.h"
#include "items/ItemTypeRegistry.h"
#include "layout/Presets.h"
#include "ui/DashboardDialog.h"
#include "ui/FieldForm.h"
#include "ui/GroupDialog.h"
#include "ui/ItemDialog.h"
#include "ui/ProjectDialog.h"
#include "ui/TaskDialog.h"
#include "ui/WebProfilesDialog.h"

#include <mirante/ItemEditor.h>
#include <mirante/Module.h>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QToolButton>
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
const QString webType = QStringLiteral("testmod.web");

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

// A type whose views are web views.
class WebType : public ItemType
{
public:
    QString id() const override { return webType; }
    QString name() const override { return QStringLiteral("Web type"); }
    QIcon icon() const override { return {}; }
    Rendering rendering() const override { return Rendering::Web; }
};

WebProfile profile(const QString &id, const QString &name)
{
    WebProfile result;
    result.id = id;
    result.name = name;
    return result;
}

void registerTestModule(ModuleRegistrar &registrar)
{
    registrar.addItemType(std::make_unique<FormType>());
    registrar.addItemType(std::make_unique<EditorType>());
    registrar.addItemType(std::make_unique<WebType>());
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
    void groupConnections();
    void groupWebProfile();
    void groupColour();
    void project();
    void projectFolders();
    void projectMainFolder();
    void projectNewFolder();
    void task();
    void itemWithForm();
    void itemWithOwnEditor();
    void itemChoosingType();
    void itemTypeGridStart();
    void itemOfMissingModule();
    void itemWebProfile();
    void webProfiles();
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

void TestDialogs::groupColour()
{
    // Work has no colour of its own, so its palette colour is shown as chosen.
    const Group original = *model.group(workGroup);
    QVERIFY(original.colour.isEmpty());
    GroupDialog dialog(original);
    const QString fallback = groupcolours::fallback(original.id);
    const int fallbackIndex = int(groupcolours::palette().indexOf(fallback));
    QCOMPARE(dialog.colour(), fallback);
    const auto swatch = [&](int i) {
        return dialog.findChild<QToolButton *>(QStringLiteral("colourSwatch%1").arg(i));
    };
    auto *custom = dialog.findChild<QToolButton *>(QStringLiteral("customColour"));
    QVERIFY(swatch(fallbackIndex));
    QVERIFY(custom);
    QVERIFY(dialog.findChild<QToolButton *>(QStringLiteral("otherColour")));
    QVERIFY(swatch(fallbackIndex)->isChecked());
    QVERIFY(!custom->isVisibleTo(&dialog));
    // Nothing chosen: the group stays without a colour of its own.
    QCOMPARE(dialog.value(), original);

    // A palette swatch.
    const int other = (fallbackIndex + 1) % int(groupcolours::palette().size());
    swatch(other)->click();
    QVERIFY(swatch(other)->isChecked());
    QVERIFY(!swatch(fallbackIndex)->isChecked());
    QCOMPARE(dialog.value().colour, groupcolours::palette().at(other));

    // A colour outside the palette, as from "Other…", gets its own swatch.
    dialog.setColour(QStringLiteral("#ABCDEF"));
    QCOMPARE(dialog.value().colour, QStringLiteral("#abcdef"));
    QVERIFY(custom->isVisibleTo(&dialog));
    QVERIFY(custom->isChecked());
    QVERIFY(!swatch(other)->isChecked());

    // Back to the palette, the custom swatch hides.
    swatch(0)->click();
    QCOMPARE(dialog.value().colour, groupcolours::palette().at(0));
    QVERIFY(!custom->isVisibleTo(&dialog));

    // A group with a colour outside the palette opens with the custom swatch.
    Group coloured = original;
    coloured.colour = QStringLiteral("#123456");
    GroupDialog second(coloured);
    QCOMPARE(second.colour(), QStringLiteral("#123456"));
    auto *secondCustom = second.findChild<QToolButton *>(QStringLiteral("customColour"));
    QVERIFY(secondCustom->isVisibleTo(&second));
    QVERIFY(secondCustom->isChecked());
    QCOMPARE(second.value(), coloured);
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

void TestDialogs::groupConnections()
{
    const Group original = *model.group(workGroup);
    const QString gitlab = QStringLiteral("gitlab");
    const QString cloud = QStringLiteral("jira.cloud");
    const QString dc = QStringLiteral("jira.dc");
    GroupDialog dialog(original);
    dialog.setConnectionKinds({gitlab, cloud, dc});
    QCOMPARE(dialog.value(), original);

    // Rows: gitlab.com, Acme self-hosted, Jira Cloud, Jira DC, time tracking.
    auto *table = dialog.findChild<QTableWidget *>(QStringLiteral("connections"));
    const auto kindBox = [table](int row) { return qobject_cast<QComboBox *>(table->cellWidget(row, 0)); };
    const auto check = [table](int row) { return table->item(row, 3); };
    QVERIFY(kindBox(0));
    QCOMPARE(kindBox(0)->count(), 3);
    QCOMPARE(kindBox(0)->currentText(), gitlab);
    QVERIFY(!kindBox(4));   // time tracking: no type uses it
    QVERIFY(!(table->item(4, 1)->flags() & Qt::ItemIsEditable));
    QVERIFY(table->item(1, 1)->flags() & Qt::ItemIsEditable);

    // Edit in place.
    table->item(1, 1)->setText(QStringLiteral(" Acme GitLab "));
    table->item(1, 2)->setText(QStringLiteral("https://git.acme.example"));
    Group expected = original;
    expected.connections[1].name = QStringLiteral("Acme GitLab");
    expected.connections[1].baseUrl = QStringLiteral("https://git.acme.example");
    QCOMPARE(dialog.value(), expected);

    // One default per kind. Unchecking leaves the kind without one.
    check(1)->setCheckState(Qt::Checked);
    QCOMPARE(dialog.value().defaultConnections.value(gitlab), original.connections[1].id);
    QCOMPARE(check(0)->checkState(), Qt::Unchecked);
    check(1)->setCheckState(Qt::Unchecked);
    QVERIFY(!dialog.value().defaultConnections.contains(gitlab));
    check(1)->setCheckState(Qt::Checked);

    // Removing the default clears it. A read-only connection is not removed.
    auto *remove = dialog.findChild<QPushButton *>(QStringLiteral("removeConnection"));
    table->setCurrentCell(4, 1);
    QVERIFY(!remove->isEnabled());
    table->setCurrentCell(1, 1);
    QVERIFY(remove->isEnabled());
    remove->click();
    QCOMPARE(table->rowCount(), 4);
    QVERIFY(!dialog.value().defaultConnections.contains(gitlab));

    // Changing the kind of a default clears it. The new kind keeps its own.
    kindBox(1)->setCurrentText(dc);
    QCOMPARE(dialog.value().connections[1].kind, dc);
    QVERIFY(!dialog.value().defaultConnections.contains(cloud));
    QCOMPARE(dialog.value().defaultConnections.value(dc), original.connections[3].id);

    // A new connection gets an id, the first kind, and becomes the default of
    // a kind without one.
    dialog.findChild<QPushButton *>(QStringLiteral("addConnection"))->click();
    QCOMPARE(table->rowCount(), 5);
    const Connection added = dialog.value().connections.back();
    QVERIFY(!added.id.isEmpty());
    QCOMPARE(added.kind, gitlab);
    QCOMPARE(dialog.value().defaultConnections.value(gitlab), added.id);

    // It needs a name and a web base URL.
    dialog.setName(QStringLiteral("Work"));
    dialog.accept();
    QCOMPARE(dialog.result(), int(QDialog::Rejected));
    table->item(4, 1)->setText(QStringLiteral("GitLab (new)"));
    table->item(4, 2)->setText(QStringLiteral("gitlab.example"));
    dialog.accept();
    QCOMPARE(dialog.result(), int(QDialog::Rejected));
    table->item(4, 2)->setText(QStringLiteral("https://gitlab.example"));
    dialog.accept();
    QCOMPARE(dialog.result(), int(QDialog::Accepted));

    expected = original;
    expected.connections.erase(expected.connections.begin() + 1);
    expected.connections[1].kind = dc;
    Connection newConnection;
    newConnection.id = added.id;
    newConnection.kind = gitlab;
    newConnection.name = QStringLiteral("GitLab (new)");
    newConnection.baseUrl = QStringLiteral("https://gitlab.example");
    expected.connections.push_back(newConnection);
    expected.defaultConnections.remove(cloud);
    expected.defaultConnections.insert(gitlab, added.id);
    QCOMPARE(dialog.value(), expected);   // time tracking and its default unchanged
}

void TestDialogs::groupWebProfile()
{
    Model local;
    local.setWebProfiles({profile(QStringLiteral("work"), QStringLiteral("Work")),
                          profile(QStringLiteral("bank"), QStringLiteral("Bank"))});
    Group original = *model.group(personalGroup);
    QVERIFY(original.webProfile.isEmpty());
    GroupDialog dialog(original);
    dialog.setWebProfiles(webprofiles::all(local));
    QCOMPARE(dialog.value(), original);

    // Default first, then by name.
    auto *combo = dialog.findChild<QComboBox *>(QStringLiteral("webProfile"));
    QVERIFY(combo);
    QCOMPARE(combo->count(), 3);
    QCOMPARE(combo->itemText(0), QStringLiteral("Default"));
    QCOMPARE(combo->itemText(1), QStringLiteral("Bank"));
    QCOMPARE(combo->currentIndex(), 0);

    dialog.setWebProfile(QStringLiteral("work"));
    QCOMPARE(dialog.value().webProfile, QStringLiteral("work"));
    // Default is stored as no profile.
    dialog.setWebProfile(kDefaultWebProfile);
    QVERIFY(dialog.value().webProfile.isEmpty());

    // A profile that is not listed is kept until another is chosen.
    original.webProfile = QStringLiteral("gone");
    GroupDialog missing(original);
    missing.setWebProfiles(webprofiles::all(local));
    auto *missingCombo = missing.findChild<QComboBox *>(QStringLiteral("webProfile"));
    QCOMPARE(missingCombo->count(), 4);
    QCOMPARE(missingCombo->currentData().toString(), QStringLiteral("gone"));
    QCOMPARE(missing.value(), original);
}

void TestDialogs::itemWebProfile()
{
    Model local;
    local.setWebProfiles({profile(QStringLiteral("work"), QStringLiteral("Work"))});

    // A web item offers the group's profile first, then every profile.
    Item item;
    item.id = QStringLiteral("i5");
    item.type = webType;
    item.name = QStringLiteral("Page");
    ItemDialog dialog(item, false, types, {});
    dialog.setWebProfiles(webprofiles::all(local), QStringLiteral("Work"));
    QVERIFY(dialog.showsWebProfile());
    auto *combo = dialog.findChild<QComboBox *>(QStringLiteral("webProfile"));
    QVERIFY(combo);
    QCOMPARE(combo->count(), 3);
    QCOMPARE(combo->itemText(0), QStringLiteral("The group's profile (Work)"));
    QCOMPARE(combo->itemText(1), QStringLiteral("Default"));
    QCOMPARE(dialog.value(), item);

    dialog.setWebProfile(kDefaultWebProfile);
    QCOMPARE(dialog.value().webProfile, kDefaultWebProfile);
    dialog.setWebProfile(QStringLiteral("work"));
    QCOMPARE(dialog.value().webProfile, QStringLiteral("work"));
    dialog.setWebProfile(QString());
    QVERIFY(dialog.value().webProfile.isEmpty());

    // A type whose views are not web views has no profile, and keeps what it had.
    Item native;
    native.id = QStringLiteral("i6");
    native.type = editorType;
    native.name = QStringLiteral("Own");
    native.webProfile = QStringLiteral("work");
    ItemDialog nativeDialog(native, false, types, {});
    nativeDialog.setWebProfiles(webprofiles::all(local), QStringLiteral("Default"));
    QVERIFY(!nativeDialog.showsWebProfile());
    QCOMPARE(nativeDialog.value().webProfile, QStringLiteral("work"));

    // A new item shows the row only while a web type is chosen.
    Item fresh;
    fresh.id = QStringLiteral("i7");
    ItemDialog creating(fresh, true, types, {});
    creating.setWebProfiles(webprofiles::all(local), QStringLiteral("Default"));
    QVERIFY(!creating.showsWebProfile());
    creating.setType(webType);
    QVERIFY(creating.showsWebProfile());
    creating.setWebProfile(QStringLiteral("work"));
    QCOMPARE(creating.value().webProfile, QStringLiteral("work"));
    creating.setType(formType);
    QVERIFY(!creating.showsWebProfile());
    QVERIFY(creating.value().webProfile.isEmpty());
}

void TestDialogs::webProfiles()
{
    Model local;
    local.setWebProfiles({profile(QStringLiteral("work"), QStringLiteral("Work"))});
    Group group;
    group.id = QStringLiteral("g1");
    group.webProfile = QStringLiteral("work");
    local.setGroup(group);

    WebProfilesDialog dialog(local);
    QString answer;
    dialog.setAskName([&answer](const QString &, const QString &) { return answer; });
    QStringList questions;
    bool yes = false;
    dialog.setConfirm([&](const QString &question) {
        questions << question;
        return yes;
    });
    QSignalSpy deleted(&dialog, &WebProfilesDialog::profileDeleted);

    // The warning is shown.
    auto *warning = dialog.findChild<QLabel *>(QStringLiteral("warning"));
    QVERIFY(warning);
    QVERIFY(warning->text().contains(QStringLiteral("can read the logins and data of all sites")));

    // Default is first, and cannot be renamed or deleted.
    QCOMPARE(dialog.ids(), (QStringList{kDefaultWebProfile, QStringLiteral("work")}));
    dialog.select(kDefaultWebProfile);
    QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("renameProfile"))->isEnabled());
    QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("deleteProfile"))->isEnabled());
    answer = QStringLiteral("Other");
    dialog.renameSelected();
    dialog.deleteSelected();
    QCOMPARE(local.webProfiles().size(), std::size_t(1));
    QVERIFY(questions.isEmpty());

    // New: a blank name, or one in use, is refused.
    answer = QStringLiteral("  ");
    dialog.addProfile();
    QCOMPARE(local.webProfiles().size(), std::size_t(1));
    answer = QStringLiteral("default");
    dialog.addProfile();
    QCOMPARE(local.webProfiles().size(), std::size_t(1));
    QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("message"))->isVisibleTo(&dialog));
    answer = QStringLiteral(" Bank ");
    dialog.addProfile();
    QCOMPARE(local.webProfiles().size(), std::size_t(2));
    const QString bank = local.webProfiles().back().id;
    QCOMPARE(local.webProfiles().back().name, QStringLiteral("Bank"));
    QCOMPARE(dialog.ids(), (QStringList{kDefaultWebProfile, bank, QStringLiteral("work")}));

    // Rename.
    dialog.select(bank);
    answer = QStringLiteral("work");
    dialog.renameSelected();
    QCOMPARE(local.webProfile(bank)->name, QStringLiteral("Bank"));
    answer = QStringLiteral("Banking");
    dialog.renameSelected();
    QCOMPARE(local.webProfile(bank)->name, QStringLiteral("Banking"));

    // Delete asks first, and names who uses the profile.
    dialog.select(QStringLiteral("work"));
    dialog.deleteSelected();
    QCOMPARE(questions.size(), 1);
    QVERIFY2(questions.front().contains(QStringLiteral("used by 1 group")), qPrintable(questions.front()));
    QVERIFY(local.webProfile(QStringLiteral("work")));
    QCOMPARE(deleted.count(), 0);
    yes = true;
    dialog.deleteSelected();
    QVERIFY(!local.webProfile(QStringLiteral("work")));
    QCOMPARE(deleted.count(), 1);
    QCOMPARE(deleted.at(0).at(0).toString(), QStringLiteral("work"));
    // The group still names it, and falls back to Default.
    QCOMPARE(local.group(QStringLiteral("g1"))->webProfile, QStringLiteral("work"));
    QCOMPARE(webprofiles::groupDefault(local, *local.group(QStringLiteral("g1"))), kDefaultWebProfile);
    QCOMPARE(dialog.ids(), (QStringList{kDefaultWebProfile, bank}));

    // Clear Data works on any profile, Default too, and asks first.
    QStringList clearAsked;
    bool clearYes = false;
    dialog.setConfirmClear([&](const QString &name) {
        clearAsked << name;
        return clearYes;
    });
    QSignalSpy clearRequested(&dialog, &WebProfilesDialog::profileClearRequested);
    dialog.select(kDefaultWebProfile);
    QVERIFY(dialog.findChild<QPushButton *>(QStringLiteral("clearProfile"))->isEnabled());
    dialog.clearSelected();
    QCOMPARE(clearAsked, QStringList{QStringLiteral("Default")});
    QCOMPARE(clearRequested.count(), 0);
    clearYes = true;
    dialog.clearSelected();
    QCOMPARE(clearRequested.count(), 1);
    QCOMPARE(clearRequested.at(0).at(0).toString(), kDefaultWebProfile);
    QCOMPARE(dialog.ids(), (QStringList{kDefaultWebProfile, bank}));
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

void TestDialogs::itemChoosingType()
{
    // A new item starts on the type grid, with the first type by name chosen.
    Item item;
    item.id = QStringLiteral("i3");
    ItemDialog dialog(item, true, types, {});
    dialog.show();     // the default button presses only while it shows
    QVERIFY(dialog.choosingType());
    QVERIFY(!dialog.findChild<QComboBox *>(QStringLiteral("type")));
    auto *list = dialog.findChild<QListWidget *>(QStringLiteral("types"));
    QVERIFY(list);
    QCOMPARE(list->viewMode(), QListView::IconMode);
    QCOMPARE(list->count(), 3);
    QCOMPARE(list->item(0)->text(), QStringLiteral("Form type"));
    QCOMPARE(list->item(1)->text(), QStringLiteral("Own editor"));
    QCOMPARE(list->item(2)->text(), QStringLiteral("Web type"));
    QCOMPARE(list->currentRow(), 0);
    QVERIFY(!dialog.editor());
    QVERIFY(dialog.value().type.isEmpty());

    auto *next = dialog.findChild<QPushButton *>(QStringLiteral("next"));
    auto *back = dialog.findChild<QPushButton *>(QStringLiteral("back"));
    QVERIFY(next);
    QVERIFY(back);
    QVERIFY(next->isDefault());

    // Accepting on the type grid goes on with the selected type instead.
    list->setCurrentRow(1);
    dialog.accept();
    QCOMPARE(dialog.result(), int(QDialog::Rejected));
    QVERIFY(!dialog.choosingType());
    QCOMPARE(dialog.value().type, editorType);
    QCOMPARE(dialog.findChild<QLabel *>(QStringLiteral("typeName"))->text(), QStringLiteral("Own editor"));
    auto *editor = dynamic_cast<TestEditor *>(dialog.editor());
    QVERIFY(editor);
    QCOMPARE(dialog.value().config, (QJsonObject{{QStringLiteral("text"), QString()}}));
    QVERIFY(!next->isDefault());
    QVERIFY(dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->isDefault());

    // Back, then the same type again, keeps what was entered.
    dialog.setName(QStringLiteral("New"));
    editor->line->setText(QStringLiteral("kept"));
    back->click();
    QVERIFY(dialog.choosingType());
    QCOMPARE(list->currentRow(), 1);
    next->click();
    QVERIFY(!dialog.choosingType());
    QCOMPARE(dialog.editor(), editor);
    QCOMPARE(dialog.value().config.value(QStringLiteral("text")).toString(), QStringLiteral("kept"));

    // Another type replaces the editor and starts from its defaults.
    dialog.back();
    emit list->itemDoubleClicked(list->item(0));
    QVERIFY(!dialog.choosingType());
    QCOMPARE(dialog.value().type, formType);
    QVERIFY(qobject_cast<FieldForm *>(dialog.editor()));
    QCOMPARE(dialog.findChildren<QWidget *>(QStringLiteral("editor")).size(), 1);
    QCOMPARE(dialog.value().config,
             (QJsonObject{{QStringLiteral("url"), QStringLiteral("https://example.com")}}));
    QCOMPARE(dialog.value().name, QStringLiteral("New"));

    // setType chooses as the grid does.
    dialog.setType(editorType);
    QVERIFY(dynamic_cast<TestEditor *>(dialog.editor()));
    QCOMPARE(list->currentRow(), 1);

    // Enter on the grid presses Next.
    dialog.back();
    list->setCurrentRow(2);
    QTest::keyClick(list, Qt::Key_Return);
    QVERIFY(!dialog.choosingType());
    QCOMPARE(dialog.value().type, webType);
}

void TestDialogs::itemTypeGridStart()
{
    // A new item with a type starts with that type selected.
    Item item;
    item.id = QStringLiteral("i8");
    item.type = webType;
    ItemDialog dialog(item, true, types, {});
    auto *list = dialog.findChild<QListWidget *>(QStringLiteral("types"));
    QCOMPARE(list->currentRow(), 2);

    // An empty registry offers no type, and Next is disabled.
    ItemTypeRegistry none;
    ItemDialog empty(Item{}, true, none, {});
    QCOMPARE(empty.findChild<QListWidget *>(QStringLiteral("types"))->count(), 0);
    QVERIFY(!empty.findChild<QPushButton *>(QStringLiteral("next"))->isEnabled());
    empty.accept();
    QVERIFY(empty.choosingType());
    QCOMPARE(empty.result(), int(QDialog::Rejected));

    // Editing an item has no type page and no Back.
    Item existing;
    existing.id = QStringLiteral("i9");
    existing.type = formType;
    existing.name = QStringLiteral("Paths");
    ItemDialog editing(existing, false, types, {});
    QVERIFY(!editing.choosingType());
    QVERIFY(!editing.findChild<QListWidget *>(QStringLiteral("types")));
    QVERIFY(!editing.findChild<QPushButton *>(QStringLiteral("back")));
    editing.setType(editorType);
    QCOMPARE(editing.value().type, formType);
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
