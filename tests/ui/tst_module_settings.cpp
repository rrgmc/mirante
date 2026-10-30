// Tests the Profiles menu and module settings screens: Manage Profiles…,
// a submenu per profile with its Modules submenu, one submenu per module that
// has screens, the menu built again when profiles change, and the dialog each
// entry opens for its profile, which stores its values in the model and its
// secrets apart. See docs/ui.md#profiles-menu and docs/ui.md#modules.
#include "api/SecretBackend.h"
#include "api/SecretStore.h"
#include "core/DeviceSettings.h"
#include "core/Model.h"
#include "core/Profiles.h"
#include "core/Store.h"
#include "items/ItemTypeRegistry.h"
#include "ui/EntityActions.h"
#include "ui/FieldForm.h"
#include "ui/MainWindow.h"
#include "ui/ModuleSettingsDialog.h"
#include "ui/ProfilesDialog.h"

#include <mirante/ItemEditor.h>

#include <QAction>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace mirante;

namespace {

class PlainType : public ItemType
{
public:
    explicit PlainType(const QString &id)
        : m_id(id)
    {
    }
    QString id() const override { return m_id; }
    QString name() const override { return m_id; }
    QIcon icon() const override { return {}; }

private:
    QString m_id;
};

// A screen with a list of paths, a required name and a token.
class FieldScreen : public SettingsScreen
{
public:
    QString id() const override { return QStringLiteral("general"); }
    QString name() const override { return QStringLiteral("General Configuration"); }
    std::vector<Field> fields() const override
    {
        Field paths;
        paths.key = QStringLiteral("paths");
        paths.label = QStringLiteral("Paths");
        paths.type = Field::Type::TextList;
        Field name;
        name.key = QStringLiteral("name");
        name.label = QStringLiteral("Name");
        name.required = true;
        Field token;
        token.key = QStringLiteral("token");
        token.label = QStringLiteral("Token");
        token.type = Field::Type::Secret;
        return {paths, name, token};
    }
};

// A screen whose server field has a default, as GitLab's base URL has.
class DefaultScreen : public SettingsScreen
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
        server.defaultValue = QStringLiteral("https://example.com");
        server.placeholder = QStringLiteral("https://example.com");
        Field note;
        note.key = QStringLiteral("note");
        note.label = QStringLiteral("Note");
        return {server, note};
    }
};

// An editor of a screen's own: one line for the "label" key. It reports a
// problem while the line is empty.
class OwnEditor : public ItemEditor
{
public:
    explicit OwnEditor(QWidget *parent)
        : ItemEditor(parent)
        , line(new QLineEdit(this))
    {
        line->setObjectName(QStringLiteral("own"));
    }

    void setConfig(const QJsonObject &config) override
    {
        m_config = config;
        line->setText(config.value(QStringLiteral("label")).toString());
    }
    QJsonObject config() const override
    {
        QJsonObject config = m_config;
        config.insert(QStringLiteral("label"), line->text());
        return config;
    }
    QStringList problems() const override
    {
        return line->text().isEmpty() ? QStringList{QStringLiteral("Enter a label.")} : QStringList();
    }

    QLineEdit *line;

private:
    QJsonObject m_config;
};

class EditorScreen : public SettingsScreen
{
public:
    QString id() const override { return QStringLiteral("look"); }
    QString name() const override { return QStringLiteral("Look"); }
    ItemEditor *createEditor(QWidget *parent) const override { return new OwnEditor(parent); }
};

void registerWithScreens(ModuleRegistrar &registrar)
{
    registrar.setModuleName(QStringLiteral("Tools"));
    registrar.addItemType(std::make_unique<PlainType>(QStringLiteral("tools.thing")));
    registrar.addSettingsScreen(std::make_unique<FieldScreen>());
    registrar.addSettingsScreen(std::make_unique<EditorScreen>());
}

void registerWithout(ModuleRegistrar &registrar)
{
    registrar.addItemType(std::make_unique<PlainType>(QStringLiteral("plain.thing")));
}

struct Fixture
{
    QTemporaryDir settingsDir;
    QSettings iniSettings{settingsDir.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat};
    DeviceSettings settings{&iniSettings};
    ItemTypeRegistry types;
    Model model;
    Store dataStore{model};
    std::unique_ptr<MainWindow> window;

    explicit Fixture(bool screens)
    {
        types.registerModule(QStringLiteral("plain"), &registerWithout);
        if (screens)
            types.registerModule(QStringLiteral("tools"), &registerWithScreens);
        window = std::make_unique<MainWindow>(model, types, dataStore, settings);
    }

    QMenu *profilesMenu() const { return window->menuBar()->findChild<QMenu *>(QStringLiteral("profilesMenu")); }

    // The submenus of the Profiles menu, by their titles.
    QStringList profileTitles() const
    {
        QStringList titles;
        for (QAction *action : profilesMenu()->actions()) {
            if (action->menu())
                titles << action->menu()->title();
        }
        return titles;
    }

    QAction *entry(const QString &profileId, const QString &screenId) const
    {
        return profilesMenu()->findChild<QAction *>(
            QStringLiteral("moduleSettings.%1.tools.%2").arg(profileId, screenId));
    }
};

Profile makeProfile(const QString &id, const QString &name)
{
    Profile profile;
    profile.id = id;
    profile.name = name;
    return profile;
}

} // namespace

class TestModuleSettings : public QObject
{
    Q_OBJECT

private slots:
    // Manage Profiles… first, then a submenu per profile, Default first. Each
    // has Modules, with a submenu for each module with screens, named after
    // the module, with the screens in their order, by their names.
    void menu()
    {
        Fixture f(true);
        QMenu *profiles = f.profilesMenu();
        QVERIFY(profiles);
        QCOMPARE(profiles->title(), QStringLiteral("&Profiles"));
        const QList<QAction *> top = profiles->actions();
        QCOMPARE(top.size(), 3);
        QCOMPARE(top[0]->text(), QStringLiteral("&Manage Profiles…"));
        QVERIFY(top[1]->isSeparator());
        QMenu *standard = top[2]->menu();
        QVERIFY(standard);
        QCOMPARE(standard->title(), QStringLiteral("Default (default)"));
        QCOMPARE(standard->actions().size(), 2);
        // Set as Default follows Modules, disabled for the default itself.
        QCOMPARE(standard->actions().at(1)->text(), QStringLiteral("Set as &Default"));
        QVERIFY(!standard->actions().at(1)->isEnabled());
        QMenu *modules = standard->actions().front()->menu();
        QVERIFY(modules);
        QCOMPARE(modules->title(), QStringLiteral("&Modules"));
        QVERIFY(modules->isEnabled());
        QCOMPARE(modules->actions().size(), 1);
        QMenu *tools = modules->actions().front()->menu();
        QVERIFY(tools);
        QCOMPARE(tools->title(), QStringLiteral("Tools"));
        const QList<QAction *> entries = tools->actions();
        QCOMPARE(entries.size(), 2);
        QCOMPARE(entries[0]->text(), QStringLiteral("General Configuration…"));
        QCOMPARE(entries[0]->objectName(), QStringLiteral("moduleSettings.default.tools.general"));
        QCOMPARE(entries[1]->text(), QStringLiteral("Look…"));

        // The menu sits after View, and before Help and the Tools menu that
        // Windows and Linux have. There is no Modules menu, and File has no
        // Profiles… any more.
        QStringList titles;
        for (QAction *action : f.window->menuBar()->actions())
            titles << action->text();
        QCOMPARE(titles.indexOf(QStringLiteral("&Profiles")), titles.indexOf(QStringLiteral("&View")) + 1);
        QVERIFY(titles.indexOf(QStringLiteral("&Help")) > titles.indexOf(QStringLiteral("&Profiles")));
        const int toolsIndex = titles.indexOf(QStringLiteral("&Tools"));
        QVERIFY(toolsIndex < 0 || toolsIndex == titles.indexOf(QStringLiteral("&Profiles")) + 1);
        QVERIFY(!titles.contains(QStringLiteral("&Modules")));
        QMenu *file = f.window->menuBar()->actions().front()->menu();
        for (QAction *action : file->actions())
            QVERIFY(action->text() != QStringLiteral("&Profiles…"));
    }

    // Manage Profiles… opens the profiles dialog.
    void manageProfiles()
    {
        Fixture f(true);
        bool opened = false;
        f.window->entityActions()->setEditProfiles([&opened](ProfilesDialog &) { opened = true; });
        f.profilesMenu()->findChild<QAction *>(QStringLiteral("manageProfiles"))->trigger();
        QVERIFY(opened);
    }

    // Without screens, each profile's Modules submenu is disabled and says so.
    void noModuleSettings()
    {
        Fixture f(false);
        QMenu *modules = f.profilesMenu()->findChild<QMenu *>(QStringLiteral("modulesMenu.default"));
        QVERIFY(modules);
        QVERIFY(!modules->isEnabled());
        QCOMPARE(modules->actions().size(), 1);
        QCOMPARE(modules->actions().front()->text(), QStringLiteral("No module settings"));
        QVERIFY(!modules->actions().front()->isEnabled());
    }

    // The menu is built again when profiles are added, renamed or removed,
    // or the default changes, in the order of the list: the default first,
    // marked, then by name.
    void followsTheProfiles()
    {
        Fixture f(true);
        QCOMPARE(f.profileTitles(), QStringList{QStringLiteral("Default (default)")});
        f.model.setProfile(makeProfile(QStringLiteral("w"), QStringLiteral("Work")));
        f.model.setProfile(makeProfile(QStringLiteral("b"), QStringLiteral("bank")));
        QTRY_COMPARE(f.profileTitles(), (QStringList{QStringLiteral("Default (default)"), QStringLiteral("bank"),
                                                     QStringLiteral("Work")}));
        QVERIFY(f.entry(QStringLiteral("w"), QStringLiteral("general")));

        f.model.setProfile(makeProfile(QStringLiteral("b"), QStringLiteral("Personal")));
        QTRY_COMPARE(f.profileTitles(), (QStringList{QStringLiteral("Default (default)"), QStringLiteral("Personal"),
                                                     QStringLiteral("Work")}));

        // Set as Default in a profile's submenu.
        auto *makeDefault = f.profilesMenu()->findChild<QAction *>(QStringLiteral("setDefaultProfile.w"));
        QVERIFY(makeDefault);
        QVERIFY(makeDefault->isEnabled());
        makeDefault->trigger();
        QCOMPARE(f.model.defaultProfile(), QStringLiteral("w"));
        QTRY_COMPARE(f.profileTitles(), (QStringList{QStringLiteral("Work (default)"), QStringLiteral("Default"),
                                                     QStringLiteral("Personal")}));
        QVERIFY(!f.profilesMenu()->findChild<QAction *>(QStringLiteral("setDefaultProfile.w"))->isEnabled());

        // Removing the default makes the first of the others by name the default.
        profiles::remove(f.model, QStringLiteral("w"));
        QTRY_COMPARE(f.profileTitles(),
                     (QStringList{QStringLiteral("Default (default)"), QStringLiteral("Personal")}));
        QVERIFY(!f.entry(QStringLiteral("w"), QStringLiteral("general")));
    }

    // The entry opens a form built from the fields for its profile only,
    // shows that profile's stored values, and stores the edited ones, keeping
    // unknown keys. Other profiles are not touched.
    void entryEditsTheModel()
    {
        Fixture f(true);
        f.model.setProfile(makeProfile(QStringLiteral("w"), QStringLiteral("Work")));
        f.model.setModuleSettings(QStringLiteral("w"), QStringLiteral("tools"), QStringLiteral("general"),
                                  {{QStringLiteral("paths"), QJsonArray{QStringLiteral("a/b")}},
                                   {QStringLiteral("name"), QStringLiteral("Old")},
                                   {QStringLiteral("future"), 3}});
        f.model.setModuleSettings(kDefaultProfile, QStringLiteral("tools"), QStringLiteral("general"),
                                  {{QStringLiteral("name"), QStringLiteral("Mine")}});
        QTRY_VERIFY(f.entry(QStringLiteral("w"), QStringLiteral("general")));

        QString title;
        bool selector = true;
        f.window->entityActions()->setExec([&title, &selector](QDialog &dialog) {
            title = dialog.windowTitle();
            selector = dialog.findChild<QWidget *>(QStringLiteral("profile")) != nullptr;
            auto *form = dialog.findChild<FieldForm *>(QStringLiteral("editor"));
            if (!form)
                return false;
            auto *paths = qobject_cast<QPlainTextEdit *>(form->editorFor(QStringLiteral("paths")));
            if (!paths || paths->toPlainText() != QLatin1String("a/b"))
                return false;
            paths->setPlainText(QStringLiteral("a/b\nc/d"));
            return true;
        });
        f.entry(QStringLiteral("w"), QStringLiteral("general"))->trigger();

        QCOMPARE(title, QStringLiteral("Work: Tools General Configuration"));
        QVERIFY(!selector);
        const QJsonObject stored =
            f.model.moduleSettings(QStringLiteral("w"), QStringLiteral("tools"), QStringLiteral("general"));
        QCOMPARE(stored.value(QStringLiteral("paths")).toArray(),
                 (QJsonArray{QStringLiteral("a/b"), QStringLiteral("c/d")}));
        QCOMPARE(stored.value(QStringLiteral("name")).toString(), QStringLiteral("Old"));
        QCOMPARE(stored.value(QStringLiteral("future")).toInt(), 3);
        QCOMPARE(f.model.moduleSettings(kDefaultProfile, QStringLiteral("tools"), QStringLiteral("general")),
                 (QJsonObject{{QStringLiteral("name"), QStringLiteral("Mine")}}));

        // Default's entry is titled with Default.
        f.window->entityActions()->setExec([&title](QDialog &dialog) {
            title = dialog.windowTitle();
            return false;
        });
        f.entry(kDefaultProfile, QStringLiteral("look"))->trigger();
        QCOMPARE(title, QStringLiteral("Default: Tools Look"));

        // Cancel changes nothing.
        f.window->entityActions()->editModuleSettings(QStringLiteral("w"), QStringLiteral("tools"),
                                                      QStringLiteral("general"));
        QCOMPARE(f.model.moduleSettings(QStringLiteral("w"), QStringLiteral("tools"), QStringLiteral("general")),
                 stored);

        // An unknown screen or profile opens nothing.
        bool opened = false;
        f.window->entityActions()->setExec([&opened](QDialog &) { return opened = true; });
        f.window->entityActions()->editModuleSettings(kDefaultProfile, QStringLiteral("tools"), QStringLiteral("none"));
        f.window->entityActions()->editModuleSettings(kDefaultProfile, QStringLiteral("none"), QStringLiteral("general"));
        f.window->entityActions()->editModuleSettings(QStringLiteral("gone"), QStringLiteral("tools"),
                                                      QStringLiteral("general"));
        QVERIFY(!opened);
    }

    // A token typed into a Secret field never reaches the stored values. It
    // goes to the secret store under "<workspace>/<profile>/<module>/<key>", and the field
    // starts empty.
    void secretsStayOutOfTheValues()
    {
        Fixture f(true);
        f.model.setModuleSettings(kDefaultProfile, QStringLiteral("tools"), QStringLiteral("general"),
                                  {{QStringLiteral("name"), QStringLiteral("Old")},
                                   {QStringLiteral("token"), QStringLiteral("leaked")}});
        SecretStore secrets(makeMemoryBackend());
        QMap<QString, QString> saved;
        QObject::connect(&secrets, &SecretStore::changed, &secrets, [&saved, &secrets](const QString &key) {
            if (const std::optional<QString> value = secrets.cached(key))
                saved.insert(key, *value);
        });
        f.window->entityActions()->setSecretStore(&secrets);
        QString shown;
        f.window->entityActions()->setExec([&shown](QDialog &dialog) {
            auto *form = dialog.findChild<FieldForm *>(QStringLiteral("editor"));
            auto *token = form ? qobject_cast<QLineEdit *>(form->editorFor(QStringLiteral("token"))) : nullptr;
            if (!token)
                return false;
            shown = token->text();
            token->setText(QStringLiteral("s3cret"));
            qobject_cast<QLineEdit *>(form->editorFor(QStringLiteral("name")))->setText(QStringLiteral("New"));
            return true;
        });
        f.window->entityActions()->editModuleSettings(kDefaultProfile, QStringLiteral("tools"),
                                                      QStringLiteral("general"));

        QVERIFY(shown.isEmpty());
        QCOMPARE(saved, (QMap<QString, QString>{{QStringLiteral("default/tools/token"), QStringLiteral("s3cret")}}));
        const QJsonObject stored =
            f.model.moduleSettings(kDefaultProfile, QStringLiteral("tools"), QStringLiteral("general"));
        // A token found in the values is dropped when they are saved.
        QVERIFY(!stored.contains(QStringLiteral("token")));
        QCOMPARE(stored.value(QStringLiteral("name")).toString(), QStringLiteral("New"));
        // Each workspace keeps its own tokens.
        f.model.setWorkspaceId(QStringLiteral("w1"));
        saved.clear();
        f.window->entityActions()->editModuleSettings(kDefaultProfile, QStringLiteral("tools"),
                                                      QStringLiteral("general"));
        QCOMPARE(saved.firstKey(), QStringLiteral("w1/default/tools/token"));
    }

    // A screen with its own editor gets it in the dialog, and OK is refused
    // while it reports problems.
    void ownEditor()
    {
        const EditorScreen screen;
        ModuleSettingsDialog dialog(QStringLiteral("Default"), QStringLiteral("Tools"), screen,
                                    {{QStringLiteral("keep"), true}});
        auto *editor = dynamic_cast<OwnEditor *>(dialog.editor());
        QVERIFY(editor);
        QCOMPARE(editor->objectName(), QStringLiteral("editor"));

        dialog.accept();
        QCOMPARE(dialog.result(), int(QDialog::Rejected));
        QVERIFY(!dialog.findChild<QLabel *>(QStringLiteral("message"))->isHidden());
        QCOMPARE(dialog.findChild<QLabel *>(QStringLiteral("message"))->text(), QStringLiteral("Enter a label."));

        editor->line->setText(QStringLiteral("Mine"));
        dialog.accept();
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QCOMPARE(dialog.values(),
                 (QJsonObject{{QStringLiteral("keep"), true}, {QStringLiteral("label"), QStringLiteral("Mine")}}));
        QVERIFY(dialog.secrets().isEmpty());
    }

    // A form refuses OK while a required field is empty.
    void formProblems()
    {
        const FieldScreen screen;
        ModuleSettingsDialog dialog(QStringLiteral("Default"), QStringLiteral("Tools"), screen, {});
        dialog.accept();
        QCOMPARE(dialog.result(), int(QDialog::Rejected));
        QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("message"))->text().contains(QStringLiteral("Name")));
    }

    // A field without a stored value shows its default as its text, not
    // only as a hint, and OK stores it. A stored value is shown as it is.
    void defaultsFillMissingValues()
    {
        const DefaultScreen screen;
        ModuleSettingsDialog fresh(QStringLiteral("Default"), QStringLiteral("Tools"), screen, {});
        QCOMPARE(fresh.values().value(QStringLiteral("server")).toString(), QStringLiteral("https://example.com"));

        const QJsonObject stored{{QStringLiteral("server"), QStringLiteral("https://own.example")}};
        ModuleSettingsDialog edited(QStringLiteral("Default"), QStringLiteral("Tools"), screen, stored);
        QCOMPARE(edited.values().value(QStringLiteral("server")).toString(), QStringLiteral("https://own.example"));
    }
};

QTEST_MAIN(TestModuleSettings)
#include "tst_module_settings.moc"
