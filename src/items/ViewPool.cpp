#include "items/ViewPool.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace mirante {

namespace {

constexpr int defaultLimit = 8;

} // namespace

struct ViewPool::Private
{
    struct Entry
    {
        QObject *placement = nullptr;
        std::function<void()> unload;
        bool visible = true;
        quint64 hiddenSince = 0;     // the clock value when it became hidden
        QMetaObject::Connection destroyed;
    };

    int limit = defaultLimit;
    std::vector<Entry> entries;
    // Counts up at each hide, so the order of hiding is exact even when two
    // placements are hidden in the same millisecond.
    quint64 clock = 0;
    // True while unload callbacks run. A callback may call back into the
    // pool; the outer loop then does the unloading that is still needed.
    bool trimming = false;

    std::vector<Entry>::iterator find(QObject *placement)
    {
        return std::find_if(entries.begin(), entries.end(),
                            [placement](const Entry &e) { return e.placement == placement; });
    }

    int hiddenCount() const
    {
        return int(std::count_if(entries.begin(), entries.end(), [](const Entry &e) { return !e.visible; }));
    }

    void forget(std::vector<Entry>::iterator it)
    {
        QObject::disconnect(it->destroyed);
        entries.erase(it);
    }

    // Unloads the placements hidden longest until no more are hidden than the
    // limit allows. Each is forgotten before its callback runs, so the
    // callback sees the pool without it.
    void trim()
    {
        if (trimming)
            return;
        trimming = true;
        while (hiddenCount() > limit) {
            auto oldest = entries.end();
            for (auto it = entries.begin(); it != entries.end(); ++it) {
                if (!it->visible && (oldest == entries.end() || it->hiddenSince < oldest->hiddenSince))
                    oldest = it;
            }
            std::function<void()> unload = std::move(oldest->unload);
            forget(oldest);
            if (unload)
                unload();
        }
        trimming = false;
    }
};

ViewPool::ViewPool(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
}

ViewPool::~ViewPool()
{
    for (const Private::Entry &entry : d->entries)
        QObject::disconnect(entry.destroyed);
}

void ViewPool::setLimit(int hiddenViews)
{
    d->limit = std::max(0, hiddenViews);
    d->trim();
}

int ViewPool::limit() const { return d->limit; }

void ViewPool::add(QObject *placement, std::function<void()> unload)
{
    if (!placement)
        return;
    auto it = d->find(placement);
    if (it == d->entries.end()) {
        Private::Entry entry;
        entry.placement = placement;
        // A placement deleted without calling remove is forgotten too, so the
        // pool never calls into a deleted placement.
        entry.destroyed = connect(placement, &QObject::destroyed, this, [this, placement] { remove(placement); });
        d->entries.push_back(std::move(entry));
        it = d->entries.end() - 1;
    }
    it->unload = std::move(unload);
    it->visible = true;
}

void ViewPool::remove(QObject *placement)
{
    auto it = d->find(placement);
    if (it != d->entries.end())
        d->forget(it);
}

void ViewPool::setVisible(QObject *placement, bool visible)
{
    auto it = d->find(placement);
    if (it == d->entries.end() || it->visible == visible)
        return;
    it->visible = visible;
    if (!visible) {
        it->hiddenSince = ++d->clock;
        d->trim();
    }
}

int ViewPool::hiddenCount() const { return d->hiddenCount(); }

} // namespace mirante
