// Tests that serialise(load(fixtures)) gives back the fixture files, compared
// as parsed JSON. This covers unknown fields and the data of the missing
// timetrack module.
#include "core/Model.h"
#include "core/Store.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QTest>

#include <map>

using namespace mirante;

namespace {

// The fixture files, by path relative to the fixture folder.
std::map<QString, QJsonObject> readFixtures()
{
    std::map<QString, QJsonObject> files;
    const QDir root(QStringLiteral(MIRANTE_FIXTURES_DIR));
    const QStringList subfolders = {QStringLiteral("groups"), QStringLiteral("projects"), QStringLiteral("tasks"),
                                    QStringLiteral("dashboards")};
    for (const QString &subfolder : subfolders) {
        const QDir folder(root.filePath(subfolder));
        for (const QString &name : folder.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name)) {
            QFile file(folder.filePath(name));
            if (!file.open(QIODevice::ReadOnly))
                continue;
            files.emplace(subfolder + QLatin1Char('/') + name, QJsonDocument::fromJson(file.readAll()).object());
        }
    }
    return files;
}

QStringList keys(const std::map<QString, QJsonObject> &files)
{
    QStringList result;
    for (const auto &entry : files)
        result << entry.first;
    return result;
}

} // namespace

class TestWriteBack : public QObject
{
    Q_OBJECT

private slots:
    void writesBackFixtures();
};

void TestWriteBack::writesBackFixtures()
{
    const std::map<QString, QJsonObject> expected = readFixtures();
    QVERIFY(!expected.empty());

    Model model;
    QVERIFY(store::load(QStringLiteral(MIRANTE_FIXTURES_DIR), model).empty());
    const std::map<QString, QJsonObject> written = store::serialise(model);

    QCOMPARE(keys(written), keys(expected));
    for (const auto &[path, json] : expected) {
        const QJsonObject actual = written.at(path);
        if (actual != json) {
            qWarning().noquote() << "expected" << path << QJsonDocument(json).toJson();
            qWarning().noquote() << "written" << path << QJsonDocument(actual).toJson();
        }
        QVERIFY2(actual == json, qPrintable(path));
    }
}

QTEST_GUILESS_MAIN(TestWriteBack)
#include "tst_write_back.moc"
