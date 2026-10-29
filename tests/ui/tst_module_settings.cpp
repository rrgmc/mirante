// Tests module settings screens in the window: the Modules menu, with one
// submenu per module that has screens, and the dialog each entry opens, which
// stores its values in the model. See docs/ui.md#modules.
#include "core/DeviceSettings.h"
#include "core/Model.h"
#include "core/Store.h"
#include "items/ItemTypeRegistry.h"
#include "ui/EntityActions.h"
#include "ui/FieldForm.h"
#include "ui/MainWindow.h"
#include "ui/ModuleSettingsDialog.h"

#include <mirante/ItemEditor.h>

#include <QAction>
#include <QComboBox>
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

// A screen with a list of paths and a required name.
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
        return {paths, name};
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

    QMenu *modulesMenu() const { return window->menuBar()->findChild<QMenu *>(QStringLiteral("modulesMenu")); }
};

} // namespace

class TestModuleSettings : public QObject
{
    Q_OBJECT

private slots:
    // Only modules with screens get a submenu, named after the module, with
    // the screens in their order, by their names.
    void menu()
    {
        Fixture f(true);
        QMenu *modules = f.modulesMenu();
        QVERIFY(modules);
        QCOMPARE(modules->title(), QStringLiteral("&Modules"));
        QCOMPARE(modules->actions().size(), 1);
        QMenu *tools = modules->actions().front()->menu();
        QVERIFY(tools);
        QCOMPARE(tools->title(), QStringLiteral("Tools"));
        const QList<QAction *> entries = tools->actions();
        QCOMPARE(entries.size(), 2);
        QCOMPARE(entries[0]->text(), QStringLiteral("General Configuration…"));
        QCOMPARE(entries[0]->objectName(), QStringLiteral("moduleSettings.tools.general"));
        QCOMPARE(entries[1]->text(), QStringLiteral("Look…"));

        // The menu sits between View and Help.
        QStringList titles;
        for (QAction *action : f.window->menuBar()->actions())
            titles << action->text();
        QCOMPARE(titles.indexOf(QStringLiteral("&Modules")), titles.indexOf(QStringLiteral("&View")) + 1);
        QCOMPARE(titles.indexOf(QStringLiteral("&Help")), titles.indexOf(QStringLiteral("&Modules")) + 1);
    }

    void noMenuWithoutScreens()
    {
        Fixture f(false);
        QVERIFY(!f.modulesMenu());
        for (QAction *action : f.window->menuBar()->actions())
            QVERIFY(action->text() != QStringLiteral("&Modules"));
    }

    // The menu entry opens a form built from the fields, shows Default's
    // stored values, and stores the edited ones, keeping unknown keys.
    void entryEditsTheModel()
    {
        Fixture f(true);
        f.model.setModuleSettings(kDefaultProfile, QStringLiteral("tools"), QStringLiteral("general"),
                                  {{QStringLiteral("paths"), QJsonArray{QStringLiteral("a/b")}},
                                   {QStringLiteral("name"), QStringLiteral("Old")},
                                   {QStringLiteral("future"), 3}});

        QString title;
        QString profile;
        f.window->entityActions()->setExec([&title, &profile](QDialog &dialog) {
            title = dialog.windowTitle();
            if (auto *settings = qobject_cast<ModuleSettingsDialog *>(&dialog))
                profile = settings->profileId();
            auto *form = dialog.findChild<FieldForm *>(QStringLiteral("editor"));
            if (!form)
                return false;
            auto *paths = qobject_cast<QPlainTextEdit *>(form->editorFor(QStringLiteral("paths")));
            if (!paths || paths->toPlainText() != QLatin1String("a/b"))
                return false;
            paths->setPlainText(QStringLiteral("a/b\nc/d"));
            return true;
        });
        f.modulesMenu()->findChild<QAction *>(QStringLiteral("moduleSettings.tools.general"))->trigger();

        QCOMPARE(title, QStringLiteral("Tools: General Configuration"));
        QCOMPARE(profile, kDefaultProfile);
        const QJsonObject stored =
            f.model.moduleSettings(kDefaultProfile, QStringLiteral("tools"), QStringLiteral("general"));
        QCOMPARE(stored.value(QStringLiteral("paths")).toArray(),
                 (QJsonArray{QStringLiteral("a/b"), QStringLiteral("c/d")}));
        QCOMPARE(stored.value(QStringLiteral("name")).toString(), QStringLiteral("Old"));
        QCOMPARE(stored.value(QStringLiteral("future")).toInt(), 3);

        // Cancel changes nothing.
        f.window->entityActions()->setExec([](QDialog &) { return false; });
        f.window->entityActions()->editModuleSettings(QStringLiteral("tools"), QStringLiteral("general"));
        QCOMPARE(f.model.moduleSettings(kDefaultProfile, QStringLiteral("tools"), QStringLiteral("general")), stored);

        // An unknown screen opens nothing.
        bool opened = false;
        f.window->entityActions()->setExec([&opened](QDialog &) { return opened = true; });
        f.window->entityActions()->editModuleSettings(QStringLiteral("tools"), QStringLiteral("none"));
        f.window->entityActions()->editModuleSettings(QStringLiteral("none"), QStringLiteral("general"));
        QVERIFY(!opened);
    }

    // The profile list chooses whose values are edited. Values edited for
    // several profiles are all stored on OK; a profile only looked at is not
    // written.
    void editsEachProfile()
    {
        Fixture f(true);
        Profile work;
        work.id = QStringLiteral("work");
        work.name = QStringLiteral("Work");
        Profile bank;
        bank.id = QStringLiteral("bank");
        bank.name = QStringLiteral("Bank");
        f.model.setProfiles({work, bank});
        const QString tools = QStringLiteral("tools");
        const QString look = QStringLiteral("look");
        f.model.setModuleSettings(QStringLiteral("work"), tools, look, {{QStringLiteral("label"), QStringLiteral("W")}});

        QStringList offered;
        f.window->entityActions()->setExec([&offered](QDialog &dialog) {
            auto *settings = qobject_cast<ModuleSettingsDialog *>(&dialog);
            auto *list = dialog.findChild<QComboBox *>(QStringLiteral("profile"));
            if (!settings || !list)
                return false;
            for (int i = 0; i < list->count(); ++i)
                offered << list->itemText(i);
            auto *editor = dynamic_cast<OwnEditor *>(settings->editor());
            // Default first, empty: its label is required, so another profile
            // cannot be chosen yet.
            if (editor->line->text() != QString() || settings->setProfile(QStringLiteral("work")))
                return false;
            editor->line->setText(QStringLiteral("D"));
            // Choosing in the list works like setProfile().
            list->setCurrentIndex(list->findData(QStringLiteral("work")));
            if (settings->profileId() != QLatin1String("work") || editor->line->text() != QLatin1String("W"))
                return false;
            editor->line->setText(QStringLiteral("W2"));
            // Bank gets a label too, and back to Default keeps its edit.
            if (!settings->setProfile(QStringLiteral("bank")))
                return false;
            editor->line->setText(QStringLiteral("B"));
            editor->line->setText(QString());
            if (settings->setProfile(kDefaultProfile))
                return false;   // the empty label is refused
            editor->line->setText(QStringLiteral("B"));
            if (!settings->setProfile(kDefaultProfile) || editor->line->text() != QLatin1String("D"))
                return false;
            return true;
        });
        f.window->entityActions()->editModuleSettings(tools, look);

        QCOMPARE(offered, (QStringList{QStringLiteral("Default"), QStringLiteral("Bank"), QStringLiteral("Work")}));
        QCOMPARE(f.model.moduleSettings(kDefaultProfile, tools, look),
                 (QJsonObject{{QStringLiteral("label"), QStringLiteral("D")}}));
        QCOMPARE(f.model.moduleSettings(QStringLiteral("work"), tools, look),
                 (QJsonObject{{QStringLiteral("label"), QStringLiteral("W2")}}));
        QCOMPARE(f.model.moduleSettings(QStringLiteral("bank"), tools, look),
                 (QJsonObject{{QStringLiteral("label"), QStringLiteral("B")}}));

        // A profile only looked at is not written.
        f.window->entityActions()->setExec([](QDialog &dialog) {
            auto *settings = qobject_cast<ModuleSettingsDialog *>(&dialog);
            return settings && settings->setProfile(QStringLiteral("work")) && settings->values().empty();
        });
        const QJsonObject before = f.model.profile(QStringLiteral("work"))->toJson();
        f.window->entityActions()->editModuleSettings(tools, look);
        QCOMPARE(f.model.profile(QStringLiteral("work"))->toJson(), before);
        QCOMPARE(f.model.moduleSettings(kDefaultProfile, tools, look),
                 (QJsonObject{{QStringLiteral("label"), QStringLiteral("D")}}));
    }

    // A screen with its own editor gets it in the dialog, and OK is refused
    // while it reports problems.
    void ownEditor()
    {
        const EditorScreen screen;
        ModuleSettingsDialog dialog(QStringLiteral("Tools"), screen, {}, [](const QString &) {
            return QJsonObject{{QStringLiteral("keep"), true}};
        });
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
        const std::map<QString, QJsonObject> values = dialog.values();
        QCOMPARE(values.size(), std::size_t(1));
        QCOMPARE(values.at(kDefaultProfile),
                 (QJsonObject{{QStringLiteral("keep"), true}, {QStringLiteral("label"), QStringLiteral("Mine")}}));
    }

    // A form refuses OK while a required field is empty.
    void formProblems()
    {
        const FieldScreen screen;
        ModuleSettingsDialog dialog(QStringLiteral("Tools"), screen, {}, {});
        dialog.accept();
        QCOMPARE(dialog.result(), int(QDialog::Rejected));
        QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("message"))->text().contains(QStringLiteral("Name")));
    }
};

QTEST_MAIN(TestModuleSettings)
#include "tst_module_settings.moc"
