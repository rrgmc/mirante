// Tests SettingsDialog: it shows the device settings, saves each change at
// once and reports it, and groups its options under section headings, with
// the terminal and the modules' device settings.
#include "core/DeviceSettings.h"
#include "items/ItemTypeRegistry.h"
#include "ui/FieldForm.h"
#include "ui/SettingsDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTest>

using namespace mirante;

namespace {

// The module "tools", with a text and a boolean device setting, and the
// module "plain", with none.
void registerTools(ModuleRegistrar &registrar)
{
    registrar.setModuleName(QStringLiteral("Tools"));
    Field editor;
    editor.key = QStringLiteral("editor");
    editor.label = QStringLiteral("Editor");
    editor.defaultValue = QStringLiteral("code");
    registrar.addDeviceSetting(editor);
    Field fresh;
    fresh.key = QStringLiteral("newWindow");
    fresh.label = QStringLiteral("New window");
    fresh.type = Field::Type::Bool;
    fresh.defaultValue = false;
    registrar.addDeviceSetting(fresh);
}

void registerPlain(ModuleRegistrar &) {}

QStringList labelTexts(const QWidget &widget)
{
    QStringList texts;
    for (const QLabel *label : widget.findChildren<QLabel *>())
        texts << label->text();
    return texts;
}

} // namespace

class TestSettingsDialog : public QObject
{
    Q_OBJECT

private slots:
    void hoverTintOnByDefault()
    {
        QTemporaryDir dir;
        QSettings ini(dir.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
        DeviceSettings settings(&ini);
        SettingsDialog dialog(settings);
        QCheckBox *box = dialog.sidebarHoverTint();
        QVERIFY(box);
        QVERIFY(box->isChecked());
        QCOMPARE(box->text(), QStringLiteral("Highlight the sidebar row of the cell under the mouse"));
        QCOMPARE(dialog.windowTitle(), QStringLiteral("Settings"));
    }

    // A change is saved at once and reported, without OK.
    void changeIsSavedAtOnce()
    {
        QTemporaryDir dir;
        QSettings ini(dir.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
        DeviceSettings settings(&ini);
        SettingsDialog dialog(settings);
        QSignalSpy changed(&dialog, &SettingsDialog::changed);

        dialog.sidebarHoverTint()->setChecked(false);
        QCOMPARE(changed.count(), 1);
        QVERIFY(!settings.sidebarHoverTint());

        // A new dialog shows the saved value.
        SettingsDialog again(settings);
        QVERIFY(!again.sidebarHoverTint()->isChecked());
        again.sidebarHoverTint()->setChecked(true);
        QVERIFY(settings.sidebarHoverTint());
    }

    // "Check servers every" shows the poll interval in minutes, two by
    // default, and saves a change in seconds.
    void pollInterval()
    {
        QTemporaryDir dir;
        QSettings ini(dir.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
        DeviceSettings settings(&ini);
        SettingsDialog dialog(settings);
        QSignalSpy changed(&dialog, &SettingsDialog::changed);
        auto *box = dialog.findChild<QSpinBox *>(QStringLiteral("pollInterval"));
        QVERIFY(box);
        QCOMPARE(box->minimum(), 1);
        QCOMPARE(box->maximum(), 60);
        QCOMPARE(box->value(), 2);
        QCOMPARE(changed.count(), 0);

        box->setValue(5);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(settings.pollInterval(), 300);

        SettingsDialog again(settings);
        QCOMPARE(again.findChild<QSpinBox *>(QStringLiteral("pollInterval"))->value(), 5);

        // An interval of 30 seconds, the least the device settings keep,
        // shows as one minute.
        settings.setPollInterval(30);
        SettingsDialog third(settings);
        QCOMPARE(third.findChild<QSpinBox *>(QStringLiteral("pollInterval"))->value(), 1);
        QCOMPARE(settings.pollInterval(), 30);
    }

    // A new hotkey is saved as portable text, and Clear turns it off.
    void globalHotkey()
    {
        QTemporaryDir dir;
        QSettings ini(dir.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
        DeviceSettings settings(&ini);
        SettingsDialog dialog(settings);
        QSignalSpy changed(&dialog, &SettingsDialog::changed);
        auto *edit = dialog.findChild<QKeySequenceEdit *>(QStringLiteral("globalHotkey"));
        QVERIFY(edit);
        QCOMPARE(edit->keySequence().toString(QKeySequence::PortableText), settings.globalHotkey());
        QCOMPARE(changed.count(), 0);

        edit->setKeySequence(QKeySequence(QStringLiteral("Ctrl+Alt+K")));
        QCOMPARE(changed.count(), 1);
        QCOMPARE(settings.globalHotkey(), QStringLiteral("Ctrl+Alt+K"));

        dialog.findChild<QPushButton *>(QStringLiteral("clearGlobalHotkey"))->click();
        QVERIFY(settings.globalHotkey().isEmpty());
        QVERIFY(edit->keySequence().isEmpty());
    }

    void hotkeyError()
    {
        QTemporaryDir dir;
        QSettings ini(dir.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
        DeviceSettings settings(&ini);
        SettingsDialog dialog(settings);
        auto *label = dialog.findChild<QLabel *>(QStringLiteral("globalHotkeyError"));
        QVERIFY(label);
        QVERIFY(label->isHidden());
        dialog.setHotkeyError(QStringLiteral("taken"));
        QVERIFY(!label->isHidden());
        QCOMPARE(label->text(), QStringLiteral("taken"));
        dialog.setHotkeyError(QString());
        QVERIFY(label->isHidden());
    }

    // Without a tray the box is off and disabled, and the stored choice stays.
    void closeToTray()
    {
        QTemporaryDir dir;
        QSettings ini(dir.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
        DeviceSettings settings(&ini);
        SettingsDialog dialog(settings);
        auto *box = dialog.findChild<QCheckBox *>(QStringLiteral("closeToTray"));
        QVERIFY(box);
        QVERIFY(box->isChecked());

        dialog.setTrayAvailable(false);
        QVERIFY(!box->isChecked());
        QVERIFY(!box->isEnabled());
        QVERIFY(settings.closeToTray());

        dialog.setTrayAvailable(true);
        QVERIFY(box->isChecked());
        box->setChecked(false);
        QVERIFY(!settings.closeToTray());
    }

    // The options sit under section headings, and the dialog closes with Close.
    void sectionsAndClose()
    {
        QTemporaryDir dir;
        QSettings ini(dir.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
        DeviceSettings settings(&ini);
        SettingsDialog dialog(settings);
        QStringList texts;
        for (const QLabel *label : dialog.findChildren<QLabel *>())
            texts << label->text();
        QVERIFY(texts.contains(QStringLiteral("Sidebar")));
        QVERIFY(texts.contains(QStringLiteral("Status")));
        QVERIFY(texts.contains(QStringLiteral("Check servers every")));
        auto *buttons = dialog.findChild<QDialogButtonBox *>();
        QVERIFY(buttons && buttons->button(QDialogButtonBox::Close));

        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        buttons->button(QDialogButtonBox::Close)->click();
        QVERIFY(!dialog.isVisible());
    }
    // The terminal command is saved as typed. The section has no device name;
    // see decision 0084.
    void terminalSection()
    {
        QTemporaryDir dir;
        QSettings ini(dir.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
        DeviceSettings settings(&ini);
        SettingsDialog dialog(settings);
        QVERIFY(labelTexts(dialog).contains(QStringLiteral("Terminal")));
        QSignalSpy changed(&dialog, &SettingsDialog::changed);

        auto *command = dialog.findChild<QLineEdit *>(QStringLiteral("terminalCommand"));
        QVERIFY(command);
        QVERIFY(command->text().isEmpty());
        QVERIFY(command->placeholderText().contains(QStringLiteral("{dir}")));
        QVERIFY(command->placeholderText().contains(QStringLiteral("{command}")));
        command->setText(QStringLiteral("alacritty --working-directory {dir} -e {command}"));
        QCOMPARE(settings.terminalCommand(), QStringLiteral("alacritty --working-directory {dir} -e {command}"));
        QVERIFY(changed.count() >= 1);

        QVERIFY(!dialog.findChild<QLineEdit *>(QStringLiteral("deviceName")));

        SettingsDialog again(settings);
        QCOMPARE(again.findChild<QLineEdit *>(QStringLiteral("terminalCommand"))->text(),
                 QStringLiteral("alacritty --working-directory {dir} -e {command}"));
    }

    // One group per module with device settings, showing the stored value or
    // the default. Only a value the user changes is stored, at once.
    void modulesSection()
    {
        QTemporaryDir dir;
        QSettings ini(dir.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
        DeviceSettings settings(&ini);
        settings.setModuleSetting(QStringLiteral("tools"), QStringLiteral("newWindow"), true);
        ItemTypeRegistry types;
        types.registerModule(QStringLiteral("tools"), &registerTools);
        types.registerModule(QStringLiteral("plain"), &registerPlain);

        SettingsDialog dialog(settings);
        QVERIFY(!labelTexts(dialog).contains(QStringLiteral("Modules")));
        dialog.setModules(types);
        dialog.setModules(types);
        QVERIFY(labelTexts(dialog).contains(QStringLiteral("Modules")));
        QCOMPARE(dialog.findChildren<QGroupBox *>().size(), 1);
        auto *box = dialog.findChild<QGroupBox *>(QStringLiteral("deviceSettings.tools"));
        QVERIFY(box);
        QCOMPARE(box->title(), QStringLiteral("Tools"));
        auto *form = box->findChild<FieldForm *>();
        QVERIFY(form);
        auto *editor = qobject_cast<QLineEdit *>(form->editorFor(QStringLiteral("editor")));
        auto *newWindow = qobject_cast<QCheckBox *>(form->editorFor(QStringLiteral("newWindow")));
        QVERIFY(editor && newWindow);
        QCOMPARE(editor->text(), QStringLiteral("code"));
        QVERIFY(newWindow->isChecked());
        QVERIFY(settings.moduleSetting(QStringLiteral("tools"), QStringLiteral("editor")).isUndefined());

        QSignalSpy changed(&dialog, &SettingsDialog::changed);
        editor->setText(QStringLiteral("subl"));
        QCOMPARE(settings.moduleSetting(QStringLiteral("tools"), QStringLiteral("editor")),
                 QJsonValue(QStringLiteral("subl")));
        QVERIFY(changed.count() >= 1);
        newWindow->setChecked(false);
        QCOMPARE(settings.moduleSetting(QStringLiteral("tools"), QStringLiteral("newWindow")), QJsonValue(false));

        // Empty text removes the value, so the default applies again.
        editor->clear();
        QVERIFY(settings.moduleSetting(QStringLiteral("tools"), QStringLiteral("editor")).isUndefined());
        SettingsDialog again(settings);
        again.setModules(types);
        auto *againForm = again.findChild<FieldForm *>(QStringLiteral("deviceSettingsForm.tools"));
        QVERIFY(againForm);
        QCOMPARE(qobject_cast<QLineEdit *>(againForm->editorFor(QStringLiteral("editor")))->text(),
                 QStringLiteral("code"));
        QVERIFY(!qobject_cast<QCheckBox *>(againForm->editorFor(QStringLiteral("newWindow")))->isChecked());
    }
};

QTEST_MAIN(TestSettingsDialog)
#include "tst_settings_dialog.moc"
