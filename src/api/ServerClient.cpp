#include "api/ServerClient.h"

#include "api/ModuleDirectory.h"
#include "api/SecretStore.h"
#include "core/Model.h"

#include <mirante/ApiClient.h>

#include <QNetworkAccessManager>

// Stubs of T0. Task T2 of the M5 plan implements them.

namespace mirante {

struct ServerClients::Private
{
    const Model *model = nullptr;
    const ModuleDirectory *modules = nullptr;
    SecretStore *secrets = nullptr;
    QNetworkAccessManager *network = nullptr;
};

ServerClients::ServerClients(const Model &model, const ModuleDirectory &modules, SecretStore &secrets,
                             QNetworkAccessManager *network, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
    d->model = &model;
    d->modules = &modules;
    d->secrets = &secrets;
    d->network = network;
}

ServerClients::~ServerClients() = default;

ApiClient *ServerClients::client(const QString &profileId, const QString &moduleId)
{
    Q_UNUSED(profileId)
    Q_UNUSED(moduleId)
    return nullptr;
}

bool ServerClients::failing(const QString &profileId, const QString &moduleId) const
{
    Q_UNUSED(profileId)
    Q_UNUSED(moduleId)
    return false;
}

QList<QPair<QString, QString>> ServerClients::failingServers() const
{
    return {};
}

void ServerClients::clear()
{
}

} // namespace mirante
