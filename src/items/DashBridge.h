#pragma once

#include "items/Placement.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>

class QWidget;

namespace mirante {

// The object "dash" that a built-in panel reaches through QWebChannel. One is
// made for each panel, for the task that holds it, and sees only that task and
// the task's group. It gives data, texts and colours as JSON, ready to show,
// and never a token, a server or a profile. See
// docs/web-views.md#built-in-panels and decisions 0094 and 0095.
// Implemented in DashBridge.cpp.
class DashBridge : public QObject
{
    Q_OBJECT

public:
    // The services must outlive the bridge.
    DashBridge(const PlacementServices &services, const QString &taskId, QObject *parent = nullptr);
    ~DashBridge() override;

    // Emits changed() when the widget's palette changes, such as when the
    // system turns dark.
    void followPalette(QWidget *widget);

    // The panel's task, with its items. Empty if the task is gone.
    Q_INVOKABLE QJsonObject currentTask() const;
    // The cached status of an item of the task. Empty for any other item.
    Q_INVOKABLE QJsonObject itemStatus(const QString &itemId) const;
    // The active tasks of the task's group, by priority, then in the
    // sidebar's order. Empty for any other group.
    Q_INVOKABLE QJsonArray groupTasks(const QString &groupId) const;
    // Opens an item of the task alone, or a task of the group, in the preview.
    Q_INVOKABLE void openItem(const QString &itemId);
    Q_INVOKABLE void openTask(const QString &taskId);
    // The fixed texts of the panels, translated, by key.
    Q_INVOKABLE QJsonObject texts() const;
    // The app's colours as CSS colours, and its font, by key.
    Q_INVOKABLE QJsonObject palette() const;

signals:
    // Something a panel shows may have changed. Emitted at most once per
    // pass of the event loop.
    void changed();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
