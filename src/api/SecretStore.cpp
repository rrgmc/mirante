#include "api/SecretStore.h"

#include "api/ModuleDirectory.h"
#include "api/SecretBackend.h"
#include "core/Model.h"

// Stubs of T0. Task T1 of the M5 plan implements them.

namespace mirante {

QString secretKey(const QString &workspaceId, const QString &profileId, const QString &moduleId,
                  const QString &fieldKey)
{
    QStringList parts;
    if (!workspaceId.isEmpty())
        parts << workspaceId;
    parts << profileId << moduleId << fieldKey;
    return parts.join(QLatin1Char('/'));
}

QStringList tokenKeys(const Model &model, const ModuleDirectory &modules)
{
    Q_UNUSED(model)
    Q_UNUSED(modules)
    return {};
}

struct SecretStore::Private
{
    std::unique_ptr<SecretBackend> backend;
};

SecretStore::SecretStore(std::unique_ptr<SecretBackend> backend, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
    d->backend = std::move(backend);
}

SecretStore::~SecretStore() = default;

void SecretStore::read(const QString &key, QObject *receiver, std::function<void(std::optional<QString>)> done)
{
    Q_UNUSED(key)
    Q_UNUSED(receiver)
    Q_UNUSED(done)
}

std::optional<QString> SecretStore::cached(const QString &key) const
{
    Q_UNUSED(key)
    return std::nullopt;
}

SecretStore::State SecretStore::state(const QString &key) const
{
    Q_UNUSED(key)
    return State::Unknown;
}

void SecretStore::write(const QString &key, const QString &value)
{
    Q_UNUSED(key)
    Q_UNUSED(value)
}

void SecretStore::remove(const QString &key)
{
    Q_UNUSED(key)
}

void SecretStore::removeAll(const QStringList &keys)
{
    Q_UNUSED(keys)
}

void SecretStore::moveAll(const QString &fromWorkspaceId, const QString &toWorkspaceId,
                          const QStringList &keysWithoutWorkspace, std::function<void()> done)
{
    Q_UNUSED(fromWorkspaceId)
    Q_UNUSED(toWorkspaceId)
    Q_UNUSED(keysWithoutWorkspace)
    if (done)
        done();
}

} // namespace mirante
