#pragma once

#include <mirante/GitState.h>

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <functional>
#include <optional>

class QObject;
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

    // The local path of a project folder on this device, or empty. An empty
    // id means the project's main folder.
    virtual QString folderPath(const QString &folderId) const = 0;

    // Asks the user for the local path of a project folder on this device,
    // and stores it. An empty id means the main folder. Placements of the
    // project's items are made again afterwards. Does nothing by default.
    virtual void mapFolder(const QString &folderId) { Q_UNUSED(folderId) }

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
    // way back. The view stays alive behind it. Without a web profile, the
    // page is a message. The default, for contexts without a placement, does
    // nothing. See decision 0069.
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

    // Calls changed with the new configuration when only keys of the type's
    // viewKeys() changed, here or on another device, until receiver is
    // destroyed. Any other change makes the view again instead. The default
    // never calls it. See decision 0081.
    virtual void onConfigChanged(QObject *receiver, std::function<void(const QJsonObject &)> changed)
    {
        Q_UNUSED(receiver)
        Q_UNUSED(changed)
    }

    // A device setting the item's module declared with
    // ModuleRegistrar::addDeviceSetting(), or the field's default value if
    // none is stored on this device. Undefined for an undeclared key. See
    // decision 0079.
    virtual QJsonValue deviceSetting(const QString &key) const
    {
        Q_UNUSED(key)
        return {};
    }

    // The absolute path of a file in the data folder's files/, such as an
    // Image item's file, or empty if there is no such file. See decision 0021.
    virtual QString filePath(const QString &fileName) const
    {
        Q_UNUSED(fileName)
        return {};
    }

    // Git. See decision 0080.

    // The branch checked out in a folder, read from its .git/HEAD, following
    // the .git file of a worktree. Empty when the head is detached, or the
    // folder is not a Git working tree. Starts no process.
    virtual QString gitHead(const QString &path) const
    {
        Q_UNUSED(path)
        return {};
    }

    // Runs git status on a folder in the background, and calls done once on
    // the main thread, unless receiver is destroyed first. The default never
    // calls it.
    virtual void gitStatus(const QString &path, QObject *receiver, std::function<void(const GitState &)> done)
    {
        Q_UNUSED(path)
        Q_UNUSED(receiver)
        Q_UNUSED(done)
    }

    // Programs. A program and its arguments are passed apart, never as one
    // command line; the core quotes them for the platform. See decision 0082.

    // Starts program with arguments in a new terminal window, in dir. An
    // empty program opens the terminal alone. Uses the terminal command of the
    // device settings, else the platform's default; see
    // docs/integrations.md#terminal-launch. Returns empty on success, else a
    // message for the user.
    virtual QString launchTerminal(const QString &dir, const QString &program, const QStringList &arguments)
    {
        Q_UNUSED(dir)
        Q_UNUSED(program)
        Q_UNUSED(arguments)
        return QStringLiteral("Programs cannot be started here.");
    }

    // Starts program with arguments, detached, in dir, such as an editor.
    // Found on the PATH if it has no path. Returns as launchTerminal() does.
    virtual QString startProgram(const QString &dir, const QString &program, const QStringList &arguments)
    {
        Q_UNUSED(dir)
        Q_UNUSED(program)
        Q_UNUSED(arguments)
        return QStringLiteral("Programs cannot be started here.");
    }

    // Shows a folder in Explorer, Finder or the desktop's file manager.
    virtual void showFolder(const QString &path) { Q_UNUSED(path) }
};

} // namespace mirante
