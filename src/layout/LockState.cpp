#include "layout/LockState.h"

#include <QSet>

namespace mirante {

struct LockState::Private
{
    QSet<QString> unlocked;
};

LockState::LockState(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
}

LockState::~LockState() = default;

bool LockState::isUnlocked(const QString &dashboardId) const { return d->unlocked.contains(dashboardId); }

void LockState::setUnlocked(const QString &dashboardId, bool unlocked)
{
    if (dashboardId.isEmpty() || isUnlocked(dashboardId) == unlocked)
        return;
    if (unlocked)
        d->unlocked.insert(dashboardId);
    else
        d->unlocked.remove(dashboardId);
    emit changed(dashboardId, unlocked);
}

} // namespace mirante
