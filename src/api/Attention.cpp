#include "api/Attention.h"

#include "api/StatusCache.h"
#include "core/DeviceSettings.h"
#include "core/Model.h"

// Stubs of T0. Task T3 of the M5 plan implements them.

namespace mirante {

struct Attention::Private
{
    const Model *model = nullptr;
    StatusCache *status = nullptr;
    DeviceSettings *settings = nullptr;
};

Attention::Attention(const Model &model, StatusCache &status, DeviceSettings &settings, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
    d->model = &model;
    d->status = &status;
    d->settings = &settings;
}

Attention::~Attention() = default;

QList<AttentionReason> Attention::reasons(const QString &itemId) const
{
    Q_UNUSED(itemId)
    return {};
}

bool Attention::needsAttention(const QString &itemId) const
{
    Q_UNUSED(itemId)
    return false;
}

int Attention::taskCount(const QString &taskId) const
{
    Q_UNUSED(taskId)
    return 0;
}

int Attention::projectCount(const QString &projectId) const
{
    Q_UNUSED(projectId)
    return 0;
}

int Attention::groupCount(const QString &groupId) const
{
    Q_UNUSED(groupId)
    return 0;
}

void Attention::markSeen(const QString &itemId)
{
    Q_UNUSED(itemId)
}

} // namespace mirante
