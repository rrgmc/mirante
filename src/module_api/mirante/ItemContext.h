#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <optional>

class QWebEngineProfile;
class QWidget;

namespace mirante {

// The details of a connection, without its token. See docs/data-model.md#connection.
struct ConnectionDetails
{
    QString id;
    QString kind;               // such as "gitlab" or "jira.cloud"
    QString name;
    QUrl baseUrl;
    QJsonObject fields;         // declared by the connection kind
};

// What an item's code can reach. The core makes one for each placement, and
// it outlives the placement's view. See docs/modules.md#item-context.
class ItemContext
{
public:
    virtual ~ItemContext() = default;

    virtual QString itemName() const = 0;

    // The item's configuration. Read from the model on each call, so it is
    // current after an external change.
    virtual QJsonObject config() const = 0;

    // moduleSettings[<module id>][key] of the item's project, or undefined.
    virtual QJsonValue projectSetting(const QString &key) const = 0;

    // The values of one of the item's own module's settings screens, in the
    // item's profile. Empty if nothing is stored. See
    // docs/decisions/0050-profiles-hold-logins-connections-and-settings.md.
    virtual QJsonObject moduleSettings(const QString &screenId) const = 0;

    // The first connection found for any of kinds, tried in that order, by the
    // resolution order: the item's connectionId, the project's override, the
    // profile default. Only connections of the item's profile count. See
    // decisions 0038 and 0050 in docs/decisions/.
    virtual std::optional<ConnectionDetails> connection(const QStringList &kinds) const = 0;

    // The local path of a project folder on this device, or empty.
    virtual QString folderPath(const QString &folderId) const = 0;

    // A live web page of url, in the item's profile, with its toolbar in
    // the cell header. The normal way for a web type to show a page. See
    // docs/web-views.md.
    virtual QWidget *createWebView(const QUrl &url, QWidget *parent) = 0;

    // The web logins of the item's profile: its own, else its group's
    // default, else Default. For a module that builds its own
    // QWebEngineView. The profile holds the logins of every site its items
    // visit, not only this module's. Declared, not defined, here; such a
    // module links Qt WebEngine with QT_LIBS. See decisions 0037, 0042 and
    // 0050 in docs/decisions/.
    virtual QWebEngineProfile *profile() = 0;

    // A grey message in the middle of the cell, such as "No GitLab connection
    // in this profile", in the style of the core's placeholders.
    virtual QWidget *createMessageView(const QString &text, QWidget *parent) = 0;

    // Changes the item's configuration through the model.
    virtual void updateConfig(const QJsonObject &config) = 0;
};

} // namespace mirante
