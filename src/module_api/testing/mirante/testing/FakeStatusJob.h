#pragma once

#include <mirante/Status.h>
#include <mirante/testing/FakeApiClient.h>

#include <QJsonObject>
#include <QMap>
#include <QObject>

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
