// Tests the Modules window: Help > Modules… opens it, it lists the modules by
// name with their item types under them, and its pane describes the selected
// module or type. See docs/ui.md#modules-window and decision 0056.
#include "core/DeviceSettings.h"
#include "core/Model.h"
#include "core/Store.h"
#include "items/ItemTypeRegistry.h"
#include "ui/MainWindow.h"
#include "ui/ModulesDialog.h"

#include <QAction>
#include <QDialogButtonBox>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QPixmap>
#include <QPointer>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QTextBrowser>
#include <QTreeWidget>

#include <memory>

using namespace mirante;

namespace {

class DescribedType : public ItemType
{
public:
    DescribedType(const QString &id, const QString &name, const QString &description)
        : m_id(id)
        , m_name(name)
        , m_description(description)
    {
    }

    QString id() const override { return m_id; }
    QString name() const override { return m_name; }
    QString description() const override { return m_description; }
    QIcon icon() const override
    {
        QPixmap pixmap(16, 16);
        pixmap.fill(Qt::red);
        return QIcon(pixmap);
    }

private:
    QString m_id;
    QString m_name;
    QString m_description;
};

class Screen : public SettingsScreen
{
public:
    QString id() const override { return QStringLiteral("general"); }
    QString name() const override { return QStringLiteral("General Configuration"); }
};

// Named "Zeta Tools" by its module, so it sorts after "alpha" although its
// id sorts first.
void registerTools(ModuleRegistrar &registrar)
{
    registrar.setModuleName(QStringLiteral("Zeta Tools"));
    registrar.setModuleDescription(QStringLiteral("Tools for testing."));
    registrar.setModuleUuid(QUuid(QStringLiteral("6f1c1d8e-3c2a-4f5e-9b7d-2a4e8c0f1b3d")));
    registrar.setModuleVersion(QStringLiteral("1.2.0"));
    registrar.setModuleAuthor(QStringLiteral("Ana Lima"));
    registrar.setModuleHomepage(QUrl(QStringLiteral("https://example.com/tools")));
    registrar.addSettingsScreen(std::make_unique<Screen>());
    registrar.addItemType(std::make_unique<DescribedType>(QStringLiteral("atools.wrench"), QStringLiteral("Wrench"),
                                                          QStringLiteral("Turns bolts.")));
    registrar.addItemType(std::make_unique<DescribedType>(QStringLiteral("atools.hammer"), QStringLiteral("Hammer"),
                                                          QStringLiteral("Drives nails.")));
}

// No name, no description and no screens.
void registerPlain(ModuleRegistrar &registrar)
{
    registrar.addItemType(
        std::make_unique<DescribedType>(QStringLiteral("alpha"), QStringLiteral("Alpha"), QString()));
}

} // namespace

class TestModulesDialog : public QObject
{
    Q_OBJECT

private slots:
    // Modules by name, their types under them by name, with icons and their
    // descriptions as tooltips.
    void listsModulesAndTypes()
    {
        ItemTypeRegistry types;
        types.registerModule(QStringLiteral("atools"), &registerTools);
        types.registerModule(QStringLiteral("alpha"), &registerPlain);
        ModulesDialog dialog(types);
        QCOMPARE(dialog.windowTitle(), QStringLiteral("Modules"));

        QTreeWidget *tree = dialog.tree();
        QCOMPARE(tree->topLevelItemCount(), 2);
        QTreeWidgetItem *alpha = tree->topLevelItem(0);
        QTreeWidgetItem *tools = tree->topLevelItem(1);
        QCOMPARE(alpha->text(0), QStringLiteral("alpha"));
        QCOMPARE(alpha->data(0, Qt::UserRole).toString(), QStringLiteral("alpha"));
        QCOMPARE(tools->text(0), QStringLiteral("Zeta Tools"));
        QCOMPARE(tools->toolTip(0), QStringLiteral("Tools for testing."));
        QVERIFY(tools->isExpanded());

        QCOMPARE(tools->childCount(), 2);
        QCOMPARE(tools->child(0)->text(0), QStringLiteral("Hammer"));
        QCOMPARE(tools->child(0)->data(0, Qt::UserRole).toString(), QStringLiteral("atools.hammer"));
        QCOMPARE(tools->child(0)->toolTip(0), QStringLiteral("Drives nails."));
        QVERIFY(!tools->child(0)->icon(0).isNull());
        QCOMPARE(tools->child(1)->text(0), QStringLiteral("Wrench"));
        QCOMPARE(alpha->childCount(), 1);
    }

    // The first module is selected. A module shows its description, the
    // identity it gave, its settings screens and where to edit them, and its
    // types with theirs.
    void describesModule()
    {
        ItemTypeRegistry types;
        types.registerModule(QStringLiteral("atools"), &registerTools);
        types.registerModule(QStringLiteral("alpha"), &registerPlain);
        ModulesDialog dialog(types);

        QCOMPARE(dialog.tree()->currentItem(), dialog.tree()->topLevelItem(0));
        QString text = dialog.details()->toPlainText();
        QVERIFY2(text.contains(QStringLiteral("No settings.")), qPrintable(text));
        QVERIFY(text.contains(QStringLiteral("Alpha")));
        QVERIFY(!text.contains(QStringLiteral("Version:")));
        QVERIFY(!text.contains(QStringLiteral("UUID:")));

        dialog.tree()->setCurrentItem(dialog.tree()->topLevelItem(1));
        text = dialog.details()->toPlainText();
        QVERIFY2(text.contains(QStringLiteral("Zeta Tools")), qPrintable(text));
        QVERIFY(text.contains(QStringLiteral("Tools for testing.")));
        QVERIFY(text.contains(QStringLiteral("Version: 1.2.0")));
        QVERIFY(text.contains(QStringLiteral("Author: Ana Lima")));
        QVERIFY(text.contains(QStringLiteral("Homepage: https://example.com/tools")));
        QVERIFY(text.contains(QStringLiteral("UUID: 6f1c1d8e-3c2a-4f5e-9b7d-2a4e8c0f1b3d")));
        QVERIFY(dialog.details()->toHtml().contains(QStringLiteral("href=\"https://example.com/tools\"")));
        QVERIFY(text.contains(QStringLiteral("Settings: General Configuration")));
        QVERIFY(text.contains(QStringLiteral("Profiles > profile > Modules > Zeta Tools")));
        QVERIFY(text.contains(QStringLiteral("Hammer")));
        QVERIFY(text.contains(QStringLiteral("Drives nails.")));
        QVERIFY(text.contains(QStringLiteral("Turns bolts.")));
    }

    // A type shows its name, description and module.
    void describesType()
    {
        ItemTypeRegistry types;
        types.registerModule(QStringLiteral("atools"), &registerTools);
        ModulesDialog dialog(types);

        dialog.tree()->setCurrentItem(dialog.tree()->topLevelItem(0)->child(1));
        const QString text = dialog.details()->toPlainText();
        QVERIFY2(text.contains(QStringLiteral("Wrench")), qPrintable(text));
        QVERIFY(text.contains(QStringLiteral("Turns bolts.")));
        QVERIFY(text.contains(QStringLiteral("Module: Zeta Tools")));
        QVERIFY(!text.contains(QStringLiteral("Drives nails.")));
    }

    void noModules()
    {
        const ItemTypeRegistry types;
        ModulesDialog dialog(types);
        QCOMPARE(dialog.tree()->topLevelItemCount(), 0);
        QCOMPARE(dialog.details()->toPlainText(), QStringLiteral("This build has no modules."));
    }

    // Help > Modules… comes first in the Help menu and opens the window,
    // which is not modal. Choosing it again brings back the same window.
    void helpMenuOpensIt()
    {
        QTemporaryDir settingsDir;
        QSettings ini(settingsDir.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
        DeviceSettings settings(&ini);
        ItemTypeRegistry types;
        types.registerModule(QStringLiteral("atools"), &registerTools);
        Model model;
        Store store(model);
        MainWindow window(model, types, store, settings);

        QMenu *help = nullptr;
        for (QAction *action : window.menuBar()->actions()) {
            if (action->text() == QStringLiteral("&Help"))
                help = action->menu();
        }
        QVERIFY(help);
        auto *modules = window.findChild<QAction *>(QStringLiteral("modulesAction"));
        QVERIFY(modules);
        QCOMPARE(modules->text(), QStringLiteral("&Modules…"));
        QList<QAction *> entries = help->actions();
#ifdef Q_OS_MACOS
        entries.removeFirst();   // Settings…, which macOS shows in the application menu
#endif
        QCOMPARE(entries.value(0), modules);
        QCOMPARE(entries.value(1)->text(), QStringLiteral("Open &Sample Data"));
        QVERIFY(entries.value(2)->isSeparator());
        QCOMPARE(entries.value(3)->text(), QStringLiteral("&About Mirante"));

        modules->trigger();
        QPointer<ModulesDialog> dialog = window.findChild<ModulesDialog *>();
        QVERIFY(dialog);
        QVERIFY(dialog->isVisible());
        QVERIFY(!dialog->isModal());
        QCOMPARE(dialog->tree()->topLevelItem(0)->text(0), QStringLiteral("Zeta Tools"));
        modules->trigger();
        QCOMPARE(window.findChildren<ModulesDialog *>().size(), 1);

        // Close deletes it; the menu then makes a new one.
        dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Close)->click();
        QTRY_VERIFY(!dialog);
        modules->trigger();
        QVERIFY(window.findChild<ModulesDialog *>());
    }
};

QTEST_MAIN(TestModulesDialog)
#include "tst_modules_dialog.moc"
