#pragma once

#include <mirante/Status.h>
#include <mirante/testing/FakeApiClient.h>

#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QStringList>

#include <optional>

namespace mirante::testing {

// A StatusJob for module tests: a configuration, project settings and module
// settings set by the test, a FakeApiClient, and the status the type
// finished with. Unlike the core's jobs, it is not deleted by finish().
class FakeStatusJob : public StatusJob
{
public:
    QJsonObject configValues;
    QJsonObject projectSettings;            // the item's module's project settings
    QMap<QString, QJsonObject> screens;     // module settings, by screen id
    FakeApiClient client;

    QJsonObject config() const override { return configValues; }
    QJsonValue projectSetting(const QString &key) const override { return projectSettings.value(key); }
    QJsonObject moduleSettings(const QString &screenId) const override { return screens.value(screenId); }
    ApiClient &server() override { return client; }
    QObject *owner() override { return &m_owner; }

    // Local folder paths by folder id, the main folder under the empty id,
    // and branches by path. gitHead() records each path it was asked for.
    QMap<QString, QString> folderPaths;
    QMap<QString, QString> heads;
    QStringList headsRead;

    QString folderPath(const QString &folderId) const override { return folderPaths.value(folderId); }
    QString gitHead(const QString &path) override
    {
        headsRead.append(path);
        return heads.value(path);
    }

    void finish(const ItemStatus &status) override
    {
        ++finishCount;
        finished = status;
    }

    // The status of the last finish(), and how many times it was called.
    std::optional<ItemStatus> finished;
    int finishCount = 0;

private:
    QObject m_owner;
};

} // namespace mirante::testing
