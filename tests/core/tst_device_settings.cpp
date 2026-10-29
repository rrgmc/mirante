// Tests DeviceSettings on a QSettings in a temporary INI file: the keys M2
// used, the recent list, folder paths and the open tabs of each workspace.
#include "core/DeviceSettings.h"

#include <QDir>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace mirante;

class TestDeviceSettings : public QObject
{
    Q_OBJECT

    std::unique_ptr<QTemporaryDir> dir;
    std::unique_ptr<QSettings> settings;

    QString iniPath() const { return dir->filePath(QStringLiteral("settings.ini")); }

    // Writes the settings to the file and reads them back in a new QSettings.
    void reopen()
    {
        settings->sync();
        settings = std::make_unique<QSettings>(iniPath(), QSettings::IniFormat);
    }

private slots:
    void init()
    {
        dir = std::make_unique<QTemporaryDir>();
        QVERIFY(dir->isValid());
        settings = std::make_unique<QSettings>(iniPath(), QSettings::IniFormat);
    }

    void cleanup()
    {
        settings.reset();
        dir.reset();
    }

    void emptyByDefault()
    {
        const DeviceSettings device(settings.get());
        QVERIFY(device.lastWorkspace().isEmpty());
        QVERIFY(device.recentWorkspaces().isEmpty());
        QVERIFY(device.folderPath(QStringLiteral("f1")).isEmpty());
        QVERIFY(!device.windowSize().isValid());
        QVERIFY(!device.windowMaximized());
        QVERIFY(device.sidebarSplitterState().isEmpty());
        const DeviceSettings::Tabs tabs = device.openTabs(QStringLiteral("C:/work"));
        QVERIFY(!tabs.saved);
        QVERIFY(tabs.ids.isEmpty());
    }

    // Values written by M2 under its keys are read.
    void readsExistingKeys()
    {
        settings->setValue(QStringLiteral("workspace/last"), QStringLiteral("C:/data"));
        settings->setValue(QStringLiteral("workspace/recent"), QStringList{QStringLiteral("C:/data"), QStringLiteral("D:/old")});
        settings->setValue(QStringLiteral("window/size"), QSize(800, 600));
        settings->setValue(QStringLiteral("window/maximized"), true);
        settings->setValue(QStringLiteral("window/splitter"), QByteArray("state"));
        reopen();

        const DeviceSettings device(settings.get());
        QCOMPARE(device.lastWorkspace(), QStringLiteral("C:/data"));
        QCOMPARE(device.recentWorkspaces(), (QStringList{QStringLiteral("C:/data"), QStringLiteral("D:/old")}));
        QCOMPARE(device.windowSize(), QSize(800, 600));
        QVERIFY(device.windowMaximized());
        QCOMPARE(device.sidebarSplitterState(), QByteArray("state"));
    }

    void writesExistingKeys()
    {
        {
            DeviceSettings device(settings.get());
            device.setLastWorkspace(QStringLiteral("C:/data"));
            device.addRecentWorkspace(QStringLiteral("C:/data"));
            device.setWindowSize(QSize(1024, 768));
            device.setWindowMaximized(true);
            device.setSidebarSplitterState(QByteArray("abc"));
        }
        reopen();
        QCOMPARE(settings->value(QStringLiteral("workspace/last")).toString(), QStringLiteral("C:/data"));
        QCOMPARE(settings->value(QStringLiteral("workspace/recent")).toStringList(), QStringList{QStringLiteral("C:/data")});
        QCOMPARE(settings->value(QStringLiteral("window/size")).toSize(), QSize(1024, 768));
        QVERIFY(settings->value(QStringLiteral("window/maximized")).toBool());
        QCOMPARE(settings->value(QStringLiteral("window/splitter")).toByteArray(), QByteArray("abc"));
    }

    void recentIsMostRecentFirstWithoutDuplicates()
    {
        DeviceSettings device(settings.get());
        device.addRecentWorkspace(QStringLiteral("a"));
        device.addRecentWorkspace(QStringLiteral("b"));
        device.addRecentWorkspace(QStringLiteral("c"));
        QCOMPARE(device.recentWorkspaces(), (QStringList{QStringLiteral("c"), QStringLiteral("b"), QStringLiteral("a")}));
        device.addRecentWorkspace(QStringLiteral("a"));
        QCOMPARE(device.recentWorkspaces(), (QStringList{QStringLiteral("a"), QStringLiteral("c"), QStringLiteral("b")}));
        // An empty folder is not added.
        device.addRecentWorkspace(QString());
        QCOMPARE(device.recentWorkspaces().size(), 3);
    }

    void recentKeepsTen()
    {
        DeviceSettings device(settings.get());
        for (int i = 0; i < 12; ++i)
            device.addRecentWorkspace(QString::number(i));
        const QStringList recent = device.recentWorkspaces();
        QCOMPARE(recent.size(), 10);
        QCOMPARE(recent.first(), QStringLiteral("11"));
        QCOMPARE(recent.last(), QStringLiteral("2"));
    }

    void removeAndClearRecent()
    {
        DeviceSettings device(settings.get());
        device.addRecentWorkspace(QStringLiteral("a"));
        device.addRecentWorkspace(QStringLiteral("b"));
        device.removeRecentWorkspace(QStringLiteral("a"));
        QCOMPARE(device.recentWorkspaces(), QStringList{QStringLiteral("b")});
        device.removeRecentWorkspace(QStringLiteral("missing"));
        QCOMPARE(device.recentWorkspaces(), QStringList{QStringLiteral("b")});
        device.clearRecentWorkspaces();
        QVERIFY(device.recentWorkspaces().isEmpty());
        QVERIFY(!settings->contains(QStringLiteral("workspace/recent")));
    }

    void folderPaths()
    {
        const QString id = QStringLiteral("0b6c3c1e-8f5a-4d1e-9c1a-2f7d3e4b5a6c");
        {
            DeviceSettings device(settings.get());
            device.setFolderPath(id, QStringLiteral("C:/src/project"));
            device.setFolderPath(QStringLiteral("other"), QStringLiteral("D:/other"));
        }
        reopen();
        DeviceSettings device(settings.get());
        QCOMPARE(device.folderPath(id), QStringLiteral("C:/src/project"));
        QCOMPARE(settings->value(QStringLiteral("folders/") + id).toString(), QStringLiteral("C:/src/project"));
        QCOMPARE(device.folderPath(QStringLiteral("other")), QStringLiteral("D:/other"));
        device.setFolderPath(id, QString());
        QVERIFY(device.folderPath(id).isEmpty());
        QVERIFY(!settings->contains(QStringLiteral("folders/") + id));
        QCOMPARE(device.folderPath(QStringLiteral("other")), QStringLiteral("D:/other"));
    }

    void openTabsRoundTrip()
    {
        const QString one = dir->filePath(QStringLiteral("one"));
        const QString two = dir->filePath(QStringLiteral("two"));
        DeviceSettings::Tabs tabs;
        tabs.ids = {QStringLiteral("d1"), QStringLiteral("focus"), QStringLiteral("d2")};
        tabs.current = QStringLiteral("focus");
        tabs.previewTask = QStringLiteral("t1");
        tabs.previewDashboard = QStringLiteral("td1");
        {
            DeviceSettings device(settings.get());
            device.setOpenTabs(one, tabs);
        }
        reopen();
        const DeviceSettings device(settings.get());
        const DeviceSettings::Tabs read = device.openTabs(one);
        QVERIFY(read.saved);
        QCOMPARE(read.ids, tabs.ids);
        QCOMPARE(read.current, tabs.current);
        QCOMPARE(read.previewTask, tabs.previewTask);
        QCOMPARE(read.previewDashboard, tabs.previewDashboard);
        // Another workspace has its own tabs.
        QVERIFY(!device.openTabs(two).saved);
        // The same folder written another way finds the same tabs.
        QVERIFY(device.openTabs(one + QStringLiteral("/sub/..")).saved);
        QVERIFY(device.openTabs(one + QStringLiteral("/")).saved);
    }

    // Saving no tabs still counts as saved, and replaces what was there.
    void emptyTabsAreSaved()
    {
        const QString workspace = dir->filePath(QStringLiteral("ws"));
        DeviceSettings device(settings.get());
        DeviceSettings::Tabs tabs;
        tabs.ids = {QStringLiteral("d1")};
        tabs.current = QStringLiteral("d1");
        device.setOpenTabs(workspace, tabs);
        device.setOpenTabs(workspace, DeviceSettings::Tabs());
        reopen();
        const DeviceSettings reread(settings.get());
        const DeviceSettings::Tabs read = reread.openTabs(workspace);
        QVERIFY(read.saved);
        QVERIFY(read.ids.isEmpty());
        QVERIFY(read.current.isEmpty());
    }

    // A path with characters a settings key cannot hold.
    void openTabsUnusualPath()
    {
        const QString workspace = dir->filePath(QStringLiteral("a b [c] = d; e#f"));
        DeviceSettings device(settings.get());
        DeviceSettings::Tabs tabs;
        tabs.ids = {QStringLiteral("d1")};
        device.setOpenTabs(workspace, tabs);
        reopen();
        const DeviceSettings reread(settings.get());
        QCOMPARE(reread.openTabs(workspace).ids, tabs.ids);
    }
};

QTEST_GUILESS_MAIN(TestDeviceSettings)
#include "tst_device_settings.moc"
