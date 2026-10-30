#include "api/StatusCache.h"

#include "api/ModuleDirectory.h"
#include "api/ServerClient.h"
#include "core/DeviceSettings.h"
#include "core/Model.h"

#include <map>

// Stubs of T0. Task T3 of the M5 plan implements them.

namespace mirante {

namespace {

// An entry that never changes.
class EmptySource : public ItemStatusSource
{
public:
    ItemStatus current() const override { return {}; }
    void refresh() override {}
    void subscribe(QObject *receiver, std::function<void(const ItemStatus &)> changed) override
    {
        Q_UNUSED(receiver)
        Q_UNUSED(changed)
    }
};

} // namespace

struct StatusCache::Private
{
    Model *model = nullptr;
    const ModuleDirectory *modules = nullptr;
    ServerClients *servers = nullptr;
    DeviceSettings *settings = nullptr;
    Clock clock;
    std::map<QString, std::unique_ptr<EmptySource>> sources;
};

StatusCache::StatusCache(Model &model, const ModuleDirectory &modules, ServerClients &servers,
                         DeviceSettings &settings, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
    d->model = &model;
    d->modules = &modules;
    d->servers = &servers;
    d->settings = &settings;
}

StatusCache::~StatusCache() = default;

ItemStatus StatusCache::status(const QString &itemId) const
{
    Q_UNUSED(itemId)
    return {};
}

ItemStatusSource &StatusCache::source(const QString &itemId)
{
    auto &slot = d->sources[itemId];
    if (!slot)
        slot = std::make_unique<EmptySource>();
    return *slot;
}

void StatusCache::refresh(const QString &itemId)
{
    Q_UNUSED(itemId)
}

void StatusCache::setVisible(const QString &itemId, bool visible)
{
    Q_UNUSED(itemId)
    Q_UNUSED(visible)
}

void StatusCache::noteTaskOpened(const QString &taskId)
{
    Q_UNUSED(taskId)
}

void StatusCache::poll()
{
}

void StatusCache::setPolling(bool on)
{
    Q_UNUSED(on)
}

void StatusCache::setClock(Clock clock)
{
    d->clock = std::move(clock);
}

void StatusCache::clear()
{
}

} // namespace mirante
