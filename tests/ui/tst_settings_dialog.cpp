// Tests SettingsDialog: it shows the device settings, saves each change at
// once and reports it, and groups its options under section headings.
#include "core/DeviceSettings.h"
#include "ui/SettingsDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace mirante;

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
        auto *buttons = dialog.findChild<QDialogButtonBox *>();
        QVERIFY(buttons && buttons->button(QDialogButtonBox::Close));

        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        buttons->button(QDialogButtonBox::Close)->click();
        QVERIFY(!dialog.isVisible());
    }
};

QTEST_MAIN(TestSettingsDialog)
#include "tst_settings_dialog.moc"
