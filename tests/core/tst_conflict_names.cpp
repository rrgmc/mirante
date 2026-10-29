// Tests the recognition of conflict copies by the UUID at the start of their
// name. See docs/decisions/0031-conflict-copies-by-id-prefix.md.
#include "core/ConflictNames.h"

#include <QTest>

using namespace mirante::conflicts;

namespace {

const QString kId = QStringLiteral("3f2b8c1e-4d5a-4e6f-9a7b-1c2d3e4f5a6b");

} // namespace

class TestConflictNames : public QObject
{
    Q_OBJECT

private slots:
    void copies_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<QString>("original");

        const QString json = kId + ".json";
        const QString png = kId + ".png";
        const QString workspace = "workspace.mirante";

        // Syncthing: <name>.sync-conflict-<date>-<time>-<device>.<ext>
        QTest::newRow("syncthing json") << kId + ".sync-conflict-20261028-101500-ABCDEFG.json" << json;
        QTest::newRow("syncthing png") << kId + ".sync-conflict-20261028-101500-ABCDEFG.png" << png;
        QTest::newRow("syncthing workspace") << "workspace.sync-conflict-20261028-101500-ABCDEFG.mirante" << workspace;
        QTest::newRow("hand-made test copy") << kId + ".sync-conflict-test.json" << json;

        // OneDrive: <name>-<device>.<ext>
        QTest::newRow("onedrive json") << kId + "-DESKTOP-4F2K9QH.json" << json;
        QTest::newRow("onedrive jpg") << kId + "-LAPTOP.jpg" << kId + ".jpg";
        QTest::newRow("onedrive workspace") << "workspace-DESKTOP-4F2K9QH.mirante" << workspace;

        // Dropbox: <name> (<user>'s conflicted copy <date>).<ext>
        QTest::newRow("dropbox json") << kId + " (Rangel's conflicted copy 2026-10-28).json" << json;
        QTest::newRow("dropbox numbered")
            << kId + " (Rangel's conflicted copy 2026-10-28 (1)).json" << json;
        QTest::newRow("dropbox png") << kId + " (Rangel's conflicted copy 2026-10-28).png" << png;
        QTest::newRow("dropbox workspace") << "workspace (Rangel's conflicted copy 2026-10-28).mirante" << workspace;

        // Any other name that starts with a UUID.
        QTest::newRow("double extension") << kId + ".old.json" << json;
        QTest::newRow("upper-case id") << kId.toUpper() + " copy.json" << kId.toUpper() + ".json";
    }

    void copies()
    {
        QFETCH(QString, name);
        QFETCH(QString, original);
        QCOMPARE(originalOf(name), original);
    }

    void notCopies_data()
    {
        QTest::addColumn<QString>("name");

        QTest::newRow("entity file") << kId + ".json";
        QTest::newRow("image file") << kId + ".png";
        QTest::newRow("image with upper-case extension") << kId + ".JPG";
        QTest::newRow("workspace") << "workspace.mirante";
        QTest::newRow("empty") << "";
        QTest::newRow("other json") << "notes.json";
        QTest::newRow("short id") << "3f2b8c1e-4d5a-4e6f-9a7b-1c2d3e4f5a6.json";
        QTest::newRow("id not at start") << "copy of " + kId + ".json";
        QTest::newRow("id without extension") << kId;
        QTest::newRow("copy without extension") << kId + " copy";
        QTest::newRow("other mirante file") << "settings.mirante";
        QTest::newRow("workspace other extension") << "workspace.json";
        QTest::newRow("upper-case workspace") << "Workspace (copy).mirante";
    }

    void notCopies()
    {
        QFETCH(QString, name);
        QCOMPARE(originalOf(name), QString());
    }
};

QTEST_GUILESS_MAIN(TestConflictNames)
#include "tst_conflict_names.moc"
