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

    // The menu entry opens a form built from the fields, shows the stored
    // values, and stores the edited ones, keeping unknown keys.
    void entryEditsTheModel()
    {
        Fixture f(true);
        f.model.setModuleSettings(QStringLiteral("tools"), QStringLiteral("general"),
                                  {{QStringLiteral("paths"), QJsonArray{QStringLiteral("a/b")}},
                                   {QStringLiteral("name"), QStringLiteral("Old")},
                                   {QStringLiteral("future"), 3}});

        QString title;
        f.window->entityActions()->setExec([&title](QDialog &dialog) {
            title = dialog.windowTitle();
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
        const QJsonObject stored = f.model.moduleSettings(QStringLiteral("tools"), QStringLiteral("general"));
        QCOMPARE(stored.value(QStringLiteral("paths")).toArray(),
                 (QJsonArray{QStringLiteral("a/b"), QStringLiteral("c/d")}));
        QCOMPARE(stored.value(QStringLiteral("name")).toString(), QStringLiteral("Old"));
        QCOMPARE(stored.value(QStringLiteral("future")).toInt(), 3);

        // Cancel changes nothing.
        f.window->entityActions()->setExec([](QDialog &) { return false; });
        f.window->entityActions()->editModuleSettings(QStringLiteral("tools"), QStringLiteral("general"));
        QCOMPARE(f.model.moduleSettings(QStringLiteral("tools"), QStringLiteral("general")), stored);

        // An unknown screen opens nothing.
        bool opened = false;
        f.window->entityActions()->setExec([&opened](QDialog &) { return opened = true; });
        f.window->entityActions()->editModuleSettings(QStringLiteral("tools"), QStringLiteral("none"));
        f.window->entityActions()->editModuleSettings(QStringLiteral("none"), QStringLiteral("general"));
        QVERIFY(!opened);
    }

    // A screen with its own editor gets it in the dialog, and OK is refused
    // while it reports problems.
    void ownEditor()
    {
        const EditorScreen screen;
        ModuleSettingsDialog dialog(QStringLiteral("Tools"), screen, {{QStringLiteral("keep"), true}});
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
        QCOMPARE(dialog.values(), (QJsonObject{{QStringLiteral("keep"), true}, {QStringLiteral("label"), QStringLiteral("Mine")}}));
    }

    // A form refuses OK while a required field is empty.
    void formProblems()
    {
        const FieldScreen screen;
        ModuleSettingsDialog dialog(QStringLiteral("Tools"), screen, {});
        dialog.accept();
        QCOMPARE(dialog.result(), int(QDialog::Rejected));
        QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("message"))->text().contains(QStringLiteral("Name")));
    }
};

QTEST_MAIN(TestModuleSettings)
#include "tst_module_settings.moc"
