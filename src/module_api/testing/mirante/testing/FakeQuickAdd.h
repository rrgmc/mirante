#pragma once

#include <mirante/QuickAdd.h>
#include <mirante/testing/FakeApiClient.h>

#include <QJsonObject>
#include <QMap>
#include <QObject>

#include <optional>
#include <vector>

namespace mirante::testing {

// A QuickAddRequest for module tests: the text and module settings set by
// the test, a FakeApiClient, and what the handler finished with. Unlike the
// core's requests, it is not deleted by finish().
class FakeQuickAddRequest : public QuickAddRequest
{
public:
    QString pasted;
    QMap<QString, QJsonObject> screens;     // module settings, by screen id
    FakeApiClient client;

    QString text() const override { return pasted; }
    QJsonObject moduleSettings(const QString &screenId) const override { return screens.value(screenId); }
    ApiClient *server() override { return &client; }
    QObject *owner() override { return &m_owner; }

    void finish(std::optional<QuickAddProposal> proposal) override
    {
        ++finishCount;
        finished = std::move(proposal);
    }

    // The proposal of the last finish(), and how many times it was called.
    std::optional<QuickAddProposal> finished;
    int finishCount = 0;

private:
    QObject m_owner;
};

// A RelatedItemRequest for module tests, in the same way.
class FakeRelatedItemRequest : public RelatedItemRequest
{
public:
    QStringList searched;
    std::vector<QuickAddItem> offered;
    QJsonObject projectSettings;            // the finder's module's project settings
    QMap<QString, QJsonObject> screens;
    FakeApiClient client;

    QStringList texts() const override { return searched; }
    std::vector<QuickAddItem> items() const override { return offered; }
    QJsonValue projectSetting(const QString &key) const override { return projectSettings.value(key); }
    QJsonObject moduleSettings(const QString &screenId) const override { return screens.value(screenId); }
    ApiClient *server() override { return &client; }
    QObject *owner() override { return &m_owner; }

    void finish(std::vector<QuickAddItem> items) override
    {
        ++finishCount;
        found = std::move(items);
    }

    std::vector<QuickAddItem> found;
    int finishCount = 0;

private:
    QObject m_owner;
};

} // namespace mirante::testing
