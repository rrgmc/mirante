#pragma once

#include <mirante/Status.h>

#include <QDateTime>
#include <QObject>
#include <QString>

#include <functional>
#include <memory>

namespace mirante {

class DeviceSettings;
class Model;
class ModuleDirectory;
class ServerClients;

// Polls the modules' servers for the status of items, and keeps it in
// memory. An item is due when it is in a visible cell, in a task opened in
// the last 30 minutes, or in an active Urgent or High task, and its status is
// older than the poll interval. See the M5 plan, part 4, and
// docs/integrations.md#polling-and-caching. Implemented in StatusCache.cpp.
class StatusCache : public QObject
{
    Q_OBJECT

public:
    // Everything passed must outlive this object.
    StatusCache(Model &model, const ModuleDirectory &modules, ServerClients &servers, DeviceSettings &settings,
                QObject *parent = nullptr);
    ~StatusCache() override;

    // The item's last status. State::None if none was fetched.
    ItemStatus status(const QString &itemId) const;
    // The item's entry, for its views. It lives as long as this object.
    ItemStatusSource &source(const QString &itemId);
    // Fetches the item at once.
    void refresh(const QString &itemId);

    // A placement of the item was shown or hidden. Counted per call, so two
    // visible cells of one item need two hides.
    void setVisible(const QString &itemId, bool visible);
    // A dashboard of the task was opened in a tab or a slot.
    void noteTaskOpened(const QString &taskId);

    // Fetches what is due now. The timer calls it every 15 seconds; tests
    // call it directly.
    void poll();
    // Starts or stops the timer. Off until started.
    void setPolling(bool on);

    // The time the cache works with. Defaults to the current UTC time; tests
    // set their own.
    using Clock = std::function<QDateTime()>;
    void setClock(Clock clock);

    // Drops every entry, for another workspace.
    void clear();

signals:
    void statusChanged(const QString &itemId);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
