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

class ApiClient;
class ItemStatusSource;

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
    // item's profile. Empty if nothing is stored. A module keeps the server
    // it talks to here, such as GitLab's base URL. See decisions 0050 and
    // 0054 in docs/decisions/.
    virtual QJsonObject moduleSettings(const QString &screenId) const = 0;

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

    // A grey message in the middle of the cell, such as "Set the issue key in
    // the item's settings.", in the style of the core's placeholders.
    virtual QWidget *createMessageView(const QString &text, QWidget *parent) = 0;

    // Changes the item's configuration through the model.
    virtual void updateConfig(const QJsonObject &config) = 0;

    // Shows a web page in place of the view, in the item's profile, with a
    // way back. The view stays alive behind it. Does nothing where the core
    // has no web views, as in tests. See decision 0069.
    virtual void showPage(const QUrl &url) { Q_UNUSED(url) }

    // Adds an item to the task this item belongs to, and returns its id, or
    // an empty string if it could not. No dashboard changes.
    virtual QString addItemToTask(const QString &typeId, const QString &name, const QJsonObject &config)
    {
        Q_UNUSED(typeId)
        Q_UNUSED(name)
        Q_UNUSED(config)
        return {};
    }

    // The module's server in the item's profile, or null for a module that
    // registered no ServerType, or where the core keeps no servers, as in
    // tests. Include <mirante/ApiClient.h> to use it. See
    // docs/modules.md#servers-and-tokens.
    virtual ApiClient *server() { return nullptr; }

    // The item's entry in the status cache, or null for a type without
    // status, or where the core keeps none, as in tests. Include
    // <mirante/Status.h> to use it.
    virtual ItemStatusSource *status() { return nullptr; }
};

} // namespace mirante
