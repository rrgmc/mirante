// Tests DeviceSettings on a QSettings in a temporary INI file: the keys M2
// used, the recent list, folder paths, the open tabs of each workspace, the
// windows of detached dashboards, the hidden-view limit, the poll interval,
// the seen attention keys, Focus's attention order, the terminal command, the
// device name and module device settings.
#include "core/DeviceSettings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QSettings>
#include <QSysInfo>
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
        QVERIFY(!device.windowGeometry().isValid());
        QVERIFY(!device.windowMaximized());
        QVERIFY(device.sidebarHoverTint());
        QVERIFY(device.sidebarSplitterState().isEmpty());
        const DeviceSettings::Tabs tabs = device.openTabs(QStringLiteral("C:/work"));
        QVERIFY(!tabs.saved);
        QVERIFY(tabs.ids.isEmpty());
        QVERIFY(tabs.detached.isEmpty());
        QVERIFY(device.detachedGeometry(QStringLiteral("d1")).isEmpty());
        QCOMPARE(device.hiddenViewLimit(), 8);
    }

    // The --settings option: settings kept in an INI file of their own.
    void ownIniFile()
    {
        const QString file = dir->filePath(QStringLiteral("own.ini"));
        {
            DeviceSettings device(file);
            QVERIFY(device.lastWorkspace().isEmpty());
            device.setLastWorkspace(QStringLiteral("C:/test"));
        }
        QVERIFY(QFileInfo::exists(file));
        QCOMPARE(QSettings(file, QSettings::IniFormat).value(QStringLiteral("workspace/last")).toString(),
                 QStringLiteral("C:/test"));
        const DeviceSettings again(file);
        QCOMPARE(again.lastWorkspace(), QStringLiteral("C:/test"));
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
            device.setWindowGeometry(QRect(-1500, 40, 1024, 768));
            device.setWindowMaximized(true);
            device.setSidebarSplitterState(QByteArray("abc"));
        }
        reopen();
        QCOMPARE(settings->value(QStringLiteral("workspace/last")).toString(), QStringLiteral("C:/data"));
        QCOMPARE(settings->value(QStringLiteral("workspace/recent")).toStringList(), QStringList{QStringLiteral("C:/data")});
        QCOMPARE(settings->value(QStringLiteral("window/size")).toSize(), QSize(1024, 768));
        QCOMPARE(settings->value(QStringLiteral("window/geometry")).toRect(), QRect(-1500, 40, 1024, 768));
        QCOMPARE(DeviceSettings(settings.get()).windowGeometry(), QRect(-1500, 40, 1024, 768));
        QVERIFY(settings->value(QStringLiteral("window/maximized")).toBool());
        QCOMPARE(settings->value(QStringLiteral("window/splitter")).toByteArray(), QByteArray("abc"));
    }

    // The settings window's options are kept across sessions.
    void sidebarHoverTint()
    {
        {
            DeviceSettings device(settings.get());
            device.setSidebarHoverTint(false);
        }
        reopen();
        QCOMPARE(settings->value(QStringLiteral("sidebar/hoverTint")).toBool(), false);
        QVERIFY(!DeviceSettings(settings.get()).sidebarHoverTint());
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

    // After a workspace moves, everything that named its folder names the
    // new one. Other workspaces and folders outside it stay.
    // The id last seen for each workspace folder, kept apart by path.
    void workspaceIds()
    {
        const QString one = dir->filePath(QStringLiteral("one"));
        const QString two = dir->filePath(QStringLiteral("two"));
        {
            DeviceSettings device(settings.get());
            QVERIFY(device.workspaceId(one).isEmpty());
            device.setWorkspaceId(one, QStringLiteral("id-1"));
            device.setWorkspaceId(two, QStringLiteral("id-2"));
            device.setWorkspaceId(QString(), QStringLiteral("ignored"));
        }
        reopen();
        DeviceSettings device(settings.get());
        QCOMPARE(device.workspaceId(one), QStringLiteral("id-1"));
        QCOMPARE(device.workspaceId(two), QStringLiteral("id-2"));
        QVERIFY(device.workspaceId(QString()).isEmpty());
        device.setWorkspaceId(one, QString());
        QVERIFY(device.workspaceId(one).isEmpty());
    }

    void moveWorkspace()
    {
        const QString from = dir->filePath(QStringLiteral("old"));
        const QString to = dir->filePath(QStringLiteral("new"));
        const QString other = dir->filePath(QStringLiteral("other"));
        DeviceSettings::Tabs tabs;
        tabs.ids = {QStringLiteral("d1"), QStringLiteral("d2")};
        tabs.current = QStringLiteral("d2");
        tabs.detached = {QStringLiteral("d3")};
        {
            DeviceSettings device(settings.get());
            device.addRecentWorkspace(from);
            device.addRecentWorkspace(other);
            device.setLastWorkspace(from);
            device.setOpenTabs(from, tabs);
            device.setWorkspaceId(from, QStringLiteral("ws-id"));
            device.setFolderPath(QStringLiteral("inside"), from + QStringLiteral("/code"));
            device.setFolderPath(QStringLiteral("outside"), other + QStringLiteral("/code"));
            device.setFolderPath(QStringLiteral("similar"), from + QStringLiteral("er/code"));
            device.moveWorkspace(from, to);
        }
        reopen();
        const DeviceSettings device(settings.get());
        QCOMPARE(device.lastWorkspace(), to);
        QCOMPARE(device.recentWorkspaces(), (QStringList{other, to}));
        QVERIFY(!device.openTabs(from).saved);
        const DeviceSettings::Tabs read = device.openTabs(to);
        QVERIFY(read.saved);
        QCOMPARE(read.ids, tabs.ids);
        QCOMPARE(read.current, tabs.current);
        QCOMPARE(read.detached, tabs.detached);
        QVERIFY(device.workspaceId(from).isEmpty());
        QCOMPARE(device.workspaceId(to), QStringLiteral("ws-id"));
        QCOMPARE(device.folderPath(QStringLiteral("inside")), to + QStringLiteral("/code"));
        QCOMPARE(device.folderPath(QStringLiteral("outside")), other + QStringLiteral("/code"));
        QCOMPARE(device.folderPath(QStringLiteral("similar")), from + QStringLiteral("er/code"));
    }

    // The last workspace is left alone when another one moved.
    void moveOtherWorkspace()
    {
        DeviceSettings device(settings.get());
        const QString last = dir->filePath(QStringLiteral("last"));
        device.setLastWorkspace(last);
        device.moveWorkspace(dir->filePath(QStringLiteral("old")), dir->filePath(QStringLiteral("new")));
        QCOMPARE(device.lastWorkspace(), last);
        QVERIFY(device.recentWorkspaces().isEmpty());
    }

    void openTabsRoundTrip()
    {
        const QString one = dir->filePath(QStringLiteral("one"));
        const QString two = dir->filePath(QStringLiteral("two"));
        DeviceSettings::Tabs tabs;
        tabs.ids = {QStringLiteral("d1"), QStringLiteral("focus"), QStringLiteral("d2")};
        tabs.current = QStringLiteral("focus");
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
        // Another workspace has its own tabs.
        QVERIFY(!device.openTabs(two).saved);
        // The same folder written another way finds the same tabs.
        QVERIFY(device.openTabs(one + QStringLiteral("/sub/..")).saved);
        QVERIFY(device.openTabs(one + QStringLiteral("/")).saved);
    }

    // Earlier versions kept the preview tab. Saving the tabs drops those keys.
    void oldPreviewKeysAreDropped()
    {
        const QString one = dir->filePath(QStringLiteral("one"));
        DeviceSettings device(settings.get());
        device.setOpenTabs(one, DeviceSettings::Tabs{{QStringLiteral("d1")}, QStringLiteral("d1"), true, {}});
        QString group;
        for (const QString &key : settings->allKeys()) {
            if (key.endsWith(QStringLiteral("/ids")))
                group = key.chopped(4);
        }
        QVERIFY(!group.isEmpty());
        settings->setValue(group + QStringLiteral("/previewTask"), QStringLiteral("t1"));
        settings->setValue(group + QStringLiteral("/previewDashboard"), QStringLiteral("td1"));

        device.setOpenTabs(one, DeviceSettings::Tabs{{QStringLiteral("d1")}, QStringLiteral("d1"), true, {}});
        for (const QString &key : settings->allKeys())
            QVERIFY2(!key.contains(QStringLiteral("preview")), qPrintable(key));
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

    void detachedTabsRoundTrip()
    {
        const QString one = dir->filePath(QStringLiteral("one"));
        const QString two = dir->filePath(QStringLiteral("two"));
        DeviceSettings::Tabs tabs;
        tabs.ids = {QStringLiteral("d1")};
        tabs.current = QStringLiteral("d1");
        tabs.detached = {QStringLiteral("d2"), QStringLiteral("d3")};
        {
            DeviceSettings device(settings.get());
            device.setOpenTabs(one, tabs);
            DeviceSettings::Tabs other;
            other.ids = {QStringLiteral("d4")};
            device.setOpenTabs(two, other);
        }
        reopen();
        DeviceSettings device(settings.get());
        const DeviceSettings::Tabs read = device.openTabs(one);
        QCOMPARE(read.ids, tabs.ids);
        QCOMPARE(read.current, tabs.current);
        QCOMPARE(read.detached, tabs.detached);
        // Another workspace has its own list.
        QVERIFY(device.openTabs(two).detached.isEmpty());

        // Saving again without detached dashboards clears the list.
        tabs.detached.clear();
        device.setOpenTabs(one, tabs);
        reopen();
        QVERIFY(DeviceSettings(settings.get()).openTabs(one).detached.isEmpty());
    }

    void detachedGeometry()
    {
        const QString id = QStringLiteral("0b6c3c1e-8f5a-4d1e-9c1a-2f7d3e4b5a6c");
        const QByteArray geometry("\x01\xd9\xd0\xcb\x00\x03geometry", 14);
        {
            DeviceSettings device(settings.get());
            device.setDetachedGeometry(id, geometry);
            device.setDetachedGeometry(QStringLiteral("other"), QByteArray("g2"));
            // An empty id is ignored.
            device.setDetachedGeometry(QString(), QByteArray("g3"));
        }
        reopen();
        DeviceSettings device(settings.get());
        QCOMPARE(device.detachedGeometry(id), geometry);
        QCOMPARE(device.detachedGeometry(QStringLiteral("other")), QByteArray("g2"));
        QVERIFY(device.detachedGeometry(QString()).isEmpty());
        QVERIFY(device.detachedGeometry(QStringLiteral("missing")).isEmpty());
        // An empty geometry removes the entry.
        device.setDetachedGeometry(id, QByteArray());
        QVERIFY(device.detachedGeometry(id).isEmpty());
        QVERIFY(!settings->contains(QStringLiteral("detached/") + id));
        QCOMPARE(device.detachedGeometry(QStringLiteral("other")), QByteArray("g2"));
    }

    void hiddenViewLimit()
    {
        {
            DeviceSettings device(settings.get());
            device.setHiddenViewLimit(3);
        }
        reopen();
        DeviceSettings device(settings.get());
        QCOMPARE(device.hiddenViewLimit(), 3);
        device.setHiddenViewLimit(20);
        QCOMPARE(device.hiddenViewLimit(), 20);
    }

    // The limit is set by hand in the settings file, so a value that is not a
    // positive number counts as not set.
    void hiddenViewLimitFromFile_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<int>("limit");
        QTest::newRow("number") << QStringLiteral("2") << 2;
        QTest::newRow("zero") << QStringLiteral("0") << 8;
        QTest::newRow("negative") << QStringLiteral("-4") << 8;
        QTest::newRow("text") << QStringLiteral("many") << 8;
        QTest::newRow("empty") << QString() << 8;
    }

    void hiddenViewLimitFromFile()
    {
        QFETCH(QString, text);
        QFETCH(int, limit);
        settings.reset();
        {
            QFile file(iniPath());
            QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
            file.write("[webViews]\nhiddenLimit=" + text.toUtf8() + "\n");
        }
        settings = std::make_unique<QSettings>(iniPath(), QSettings::IniFormat);
        QCOMPARE(DeviceSettings(settings.get()).hiddenViewLimit(), limit);
    }

    void pollInterval()
    {
        {
            DeviceSettings device(settings.get());
            QCOMPARE(device.pollInterval(), 120);
            device.setPollInterval(300);
        }
        reopen();
        DeviceSettings device(settings.get());
        QCOMPARE(device.pollInterval(), 300);
        // Kept between 30 seconds and one hour.
        device.setPollInterval(5);
        QCOMPARE(device.pollInterval(), 30);
        device.setPollInterval(100000);
        QCOMPARE(device.pollInterval(), 3600);
    }

    void pollIntervalFromFile_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<int>("seconds");
        QTest::newRow("number") << QStringLiteral("600") << 600;
        QTest::newRow("too small") << QStringLiteral("1") << 30;
        QTest::newRow("too large") << QStringLiteral("99999") << 3600;
        QTest::newRow("text") << QStringLiteral("often") << 120;
        QTest::newRow("empty") << QString() << 120;
    }

    void pollIntervalFromFile()
    {
        QFETCH(QString, text);
        QFETCH(int, seconds);
        settings.reset();
        {
            QFile file(iniPath());
            QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
            file.write("[status]\npollInterval=" + text.toUtf8() + "\n");
        }
        settings = std::make_unique<QSettings>(iniPath(), QSettings::IniFormat);
        QCOMPARE(DeviceSettings(settings.get()).pollInterval(), seconds);
    }

    void seenAttention()
    {
        const QString ws = QStringLiteral("8d0c2f4e-1b7a-4c5d-9e3f-6a2b1c0d9e8f");
        const QString other = QStringLiteral("1f2e3d4c-5b6a-4978-8a9b-0c1d2e3f4a5b");
        const QString item = QStringLiteral("3c4d5e6f-7a8b-4c9d-8e0f-1a2b3c4d5e6f");
        const QStringList keys{QStringLiteral("approved"), QStringLiteral("status:10001")};
        {
            DeviceSettings device(settings.get());
            QVERIFY(!device.hasSeenAttention(ws, item));
            QVERIFY(device.seenAttention(ws, item).isEmpty());
            device.setSeenAttention(ws, item, keys);
            // An empty list is still a record.
            device.setSeenAttention(ws, QStringLiteral("empty"), {});
            // Empty ids are ignored.
            device.setSeenAttention(QString(), item, {QStringLiteral("x")});
            device.setSeenAttention(ws, QString(), {QStringLiteral("x")});
        }
        reopen();
        DeviceSettings device(settings.get());
        QVERIFY(device.hasSeenAttention(ws, item));
        QCOMPARE(device.seenAttention(ws, item), keys);
        QVERIFY(device.hasSeenAttention(ws, QStringLiteral("empty")));
        QVERIFY(device.seenAttention(ws, QStringLiteral("empty")).isEmpty());
        QVERIFY(!device.hasSeenAttention(QString(), item));
        QVERIFY(!device.hasSeenAttention(ws, QString()));
        // Each workspace has its own records.
        QVERIFY(!device.hasSeenAttention(other, item));

        device.removeSeenAttention(ws, item);
        QVERIFY(!device.hasSeenAttention(ws, item));
        QVERIFY(device.seenAttention(ws, item).isEmpty());
        QVERIFY(device.hasSeenAttention(ws, QStringLiteral("empty")));
    }

    void focusAttentionFirst()
    {
        {
            DeviceSettings device(settings.get());
            QVERIFY(!device.focusAttentionFirst());
            device.setFocusAttentionFirst(true);
        }
        reopen();
        DeviceSettings device(settings.get());
        QVERIFY(device.focusAttentionFirst());
        device.setFocusAttentionFirst(false);
        QVERIFY(!device.focusAttentionFirst());
    }

    // A hotkey is set unless the user cleared it, which is kept as empty.
    void globalHotkey()
    {
        {
            DeviceSettings device(settings.get());
            QVERIFY(device.globalHotkey().endsWith(QStringLiteral("+Shift+D")));
            device.setGlobalHotkey(QStringLiteral("Ctrl+Alt+K"));
        }
        reopen();
        DeviceSettings device(settings.get());
        QCOMPARE(device.globalHotkey(), QStringLiteral("Ctrl+Alt+K"));
        device.setGlobalHotkey(QString());
        QVERIFY(device.globalHotkey().isEmpty());
    }

    void closeToTray()
    {
        {
            DeviceSettings device(settings.get());
            QVERIFY(device.closeToTray());
            device.setCloseToTray(false);
        }
        reopen();
        DeviceSettings device(settings.get());
        QVERIFY(!device.closeToTray());
    }
    // Empty means the platform's terminal, and is not kept.
    void terminalCommand()
    {
        {
            DeviceSettings device(settings.get());
            QVERIFY(device.terminalCommand().isEmpty());
            device.setTerminalCommand(QStringLiteral("wezterm start --cwd {dir} -- {command}"));
        }
        reopen();
        DeviceSettings device(settings.get());
        QCOMPARE(device.terminalCommand(), QStringLiteral("wezterm start --cwd {dir} -- {command}"));
        device.setTerminalCommand(QStringLiteral("  "));
        QVERIFY(device.terminalCommand().isEmpty());
        QVERIFY(!settings->contains(QStringLiteral("terminal/command")));
    }

    // The host name until a name is set; an empty name goes back to it.
    void deviceName()
    {
        {
            DeviceSettings device(settings.get());
            QCOMPARE(device.deviceName(), QSysInfo::machineHostName());
            device.setDeviceName(QStringLiteral(" Laptop "));
        }
        reopen();
        DeviceSettings device(settings.get());
        QCOMPARE(device.deviceName(), QStringLiteral("Laptop"));
        device.setDeviceName(QString());
        QCOMPARE(device.deviceName(), QSysInfo::machineHostName());
    }

    void moduleSettingsKeepTheirType_data()
    {
        QTest::addColumn<QJsonValue>("value");
        QTest::newRow("text") << QJsonValue(QStringLiteral("code"));
        QTest::newRow("text that looks like a number") << QJsonValue(QStringLiteral("42"));
        QTest::newRow("text that looks like a boolean") << QJsonValue(QStringLiteral("true"));
        QTest::newRow("text with quotes and commas") << QJsonValue(QStringLiteral("say \"hi\", then go"));
        QTest::newRow("empty text") << QJsonValue(QString());
        QTest::newRow("integer") << QJsonValue(42);
        QTest::newRow("boolean") << QJsonValue(false);
        QTest::newRow("array") << QJsonValue(QJsonArray{1, QStringLiteral("a")});
        QTest::newRow("object") << QJsonValue(QJsonObject{{QStringLiteral("k"), true}});
    }

    // Values read back as they were stored, also from an INI file, which
    // keeps every value as text.
    void moduleSettingsKeepTheirType()
    {
        QFETCH(QJsonValue, value);
        {
            DeviceSettings device(settings.get());
            QVERIFY(device.moduleSetting(QStringLiteral("folder"), QStringLiteral("editor")).isUndefined());
            device.setModuleSetting(QStringLiteral("folder"), QStringLiteral("editor"), value);
        }
        reopen();
        QVERIFY(settings->contains(QStringLiteral("modules/folder/editor")));
        const DeviceSettings device(settings.get());
        QCOMPARE(device.moduleSetting(QStringLiteral("folder"), QStringLiteral("editor")), value);
        QVERIFY(device.moduleSetting(QStringLiteral("claude"), QStringLiteral("editor")).isUndefined());
    }

    void moduleSettingRemovedByNullOrUndefined()
    {
        DeviceSettings device(settings.get());
        device.setModuleSetting(QStringLiteral("claude"), QStringLiteral("program"), QStringLiteral("claude"));
        device.setModuleSetting(QStringLiteral("claude"), QStringLiteral("program"), QJsonValue(QJsonValue::Null));
        QVERIFY(!settings->contains(QStringLiteral("modules/claude/program")));
        device.setModuleSetting(QStringLiteral("claude"), QStringLiteral("program"), 3);
        device.setModuleSetting(QStringLiteral("claude"), QStringLiteral("program"), QJsonValue());
        QVERIFY(device.moduleSetting(QStringLiteral("claude"), QStringLiteral("program")).isUndefined());
    }

    // A value typed into the file by hand without JSON quotes reads as text.
    void moduleSettingNotJson()
    {
        settings->setValue(QStringLiteral("modules/folder/editor"), QStringLiteral("code --new-window"));
        const DeviceSettings device(settings.get());
        QCOMPARE(device.moduleSetting(QStringLiteral("folder"), QStringLiteral("editor")),
                 QJsonValue(QStringLiteral("code --new-window")));
    }
};

QTEST_GUILESS_MAIN(TestDeviceSettings)
#include "tst_device_settings.moc"
