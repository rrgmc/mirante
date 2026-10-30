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
#include <QSpinBox>
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
};

QTEST_MAIN(TestSettingsDialog)
#include "tst_settings_dialog.moc"
