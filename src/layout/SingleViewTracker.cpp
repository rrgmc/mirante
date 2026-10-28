#include "layout/SingleViewTracker.h"

#include <QHash>
#include <QWidget>

namespace mirante {

struct SingleViewTracker::Private
{
    QHash<QString, QWidget *> holders;                  // item id to cell
    QHash<QWidget *, QMetaObject::Connection> watched;  // cells whose destruction is watched
};

SingleViewTracker::SingleViewTracker(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
}

SingleViewTracker::~SingleViewTracker()
{
    for (const QMetaObject::Connection &connection : std::as_const(d->watched))
        disconnect(connection);
}

QWidget *SingleViewTracker::holder(const QString &itemId) const { return d->holders.value(itemId); }

void SingleViewTracker::setHolder(const QString &itemId, QWidget *cell)
{
    if (!cell) {
        if (d->holders.remove(itemId))
            emit holderChanged(itemId);
        return;
    }
    if (d->holders.value(itemId) == cell)
        return;
    d->holders.insert(itemId, cell);
    // A cell that is destroyed without calling release() must not stay a holder.
    if (!d->watched.contains(cell)) {
        d->watched.insert(cell, connect(cell, &QObject::destroyed, this, [this, cell] { release(cell); }));
    }
    emit holderChanged(itemId);
}

void SingleViewTracker::release(QWidget *cell)
{
    const auto watched = d->watched.find(cell);
    if (watched != d->watched.end()) {
        disconnect(watched.value());
        d->watched.erase(watched);
    }

    QStringList released;
    for (auto it = d->holders.begin(); it != d->holders.end();) {
        if (it.value() == cell) {
            released.append(it.key());
            it = d->holders.erase(it);
        } else {
            ++it;
        }
    }
    for (const QString &itemId : std::as_const(released))
        emit holderChanged(itemId);
}

} // namespace mirante
