#pragma once

#include "items/Placement.h"

#include <mirante/ItemContext.h>

#include <QPointer>
#include <QString>

#include <functional>
#include <vector>

class QLabel;

namespace mirante {

class ItemType;

// The ItemContext of one placement: what an item's code can reach, read from
// the model, the profile manager and the device settings. Private to the items
// library; modules see only ItemContext. Implemented in ItemContextImpl.cpp.
class ItemContextImpl : public ItemContext
{
public:
    // The services must outlive the context.
    ItemContextImpl(const PlacementServices &services, const QString &taskId, const QString &itemId);
    ~ItemContextImpl() override;

    // Called with the widget a new web view puts in the cell header, its
    // toolbar. The placement sets it.
    std::function<void(QWidget *)> onHeaderWidget;
    // Called with the address of showPage(). The placement sets it. Without
    // it, as in a context of its own, showPage() does nothing.
    std::function<void(const QUrl &)> onShowPage;

    // A web page in the item's profile, or a message where there is no
    // profile. createWebView() and the placement's pages use it. toolbar is
    // set to the page's toolbar, which the page owns, or to null for a message.
    QWidget *createPage(const QUrl &url, QWidget *parent, QWidget *&toolbar);

    // A built-in panel of the item's task, such as "task-summary", in the
    // app profile, or a message where there is no profile manager. See
    // docs/web-views.md#built-in-panels.
    QWidget *createDashPanel(const QString &panel, QWidget *parent);

    QString itemName() const override;
    QJsonObject config() const override;
    QJsonValue projectSetting(const QString &key) const override;
    QJsonObject moduleSettings(const QString &screenId) const override;
    QString folderPath(const QString &folderId) const override;
    QWidget *createWebView(const QUrl &url, QWidget *parent) override;
    QWebEngineProfile *profile() override;
    QWidget *createMessageView(const QString &text, QWidget *parent) override;
    void updateConfig(const QJsonObject &config) override;
    void showPage(const QUrl &url) override;
    QString addItemToTask(const QString &typeId, const QString &name, const QJsonObject &config) override;
    ApiClient *server() override;
    ItemStatusSource *status() override;
    void mapFolder(const QString &folderId) override;
    void onConfigChanged(QObject *receiver, std::function<void(const QJsonObject &)> changed) override;
    QJsonValue deviceSetting(const QString &key) const override;
    QString filePath(const QString &fileName) const override;
    QString gitHead(const QString &path) const override;
    void gitStatus(const QString &path, QObject *receiver, std::function<void(const GitState &)> done) override;
    QString launchTerminal(const QString &dir, const QString &program, const QStringList &arguments) override;
    QString startProgram(const QString &dir, const QString &program, const QStringList &arguments) override;
    void showFolder(const QString &path) override;

    // Calls the receivers of onConfigChanged() that are still alive. The
    // placement calls it when only view keys changed. See decision 0081.
    void notifyConfigChanged(const QJsonObject &config);

private:
    struct ConfigReceiver
    {
        QPointer<QObject> receiver;
        std::function<void(const QJsonObject &)> changed;
    };
    std::vector<ConfigReceiver> m_configReceivers;

    PlacementServices m_services;
    QString m_taskId;
    QString m_itemId;
};

// The module of an item type: the part of its id before the first dot.
QString moduleOfType(const QString &typeId);

// A grey message in the middle of a cell. The core's placeholders and
// ItemContext::createMessageView use it, so they look the same.
QLabel *createMessageLabel(const QString &text, QWidget *parent);

// What the body of an item's cell says when its type makes no view: the
// missing module, the removed type, or the type, name and id of the item.
// type may be null.
QString itemPlaceholderText(const ItemType *type, const QString &itemName, const QString &itemId);

} // namespace mirante
