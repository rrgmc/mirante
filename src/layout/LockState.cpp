// Stub from task T0. Task T14 implements this file.
#include "layout/LockState.h"

namespace mirante {

struct LockState::Private
{
};

LockState::LockState(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
}

LockState::~LockState() = default;

bool LockState::isUnlocked(const QString & /*dashboardId*/) const { return false; }
void LockState::setUnlocked(const QString & /*dashboardId*/, bool /*unlocked*/) {}

} // namespace mirante
