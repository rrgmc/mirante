#pragma once

#include "items/Placement.h"

#include <mirante/ItemContext.h>

#include <QString>

#include <functional>

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

    QString itemName() const override;
    QJsonObject config() const override;
    QJsonValue projectSetting(const QString &key) const override;
    std::optional<ConnectionDetails> connection(const QStringList &kinds) const override;
    QString folderPath(const QString &folderId) const override;
    QWidget *createWebView(const QUrl &url, QWidget *parent) override;
    QWebEngineProfile *profile() override;
    QWidget *createMessageView(const QString &text, QWidget *parent) override;
    void updateConfig(const QJsonObject &config) override;

private:
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
// missing module, or the type, name and id of the item. type may be null.
QString itemPlaceholderText(const ItemType *type, const QString &itemName, const QString &itemId);

} // namespace mirante
