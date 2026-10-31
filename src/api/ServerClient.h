#pragma once

#include <QList>
#include <QObject>
#include <QPair>
#include <QString>

#include <memory>

class QNetworkAccessManager;

namespace mirante {

class ApiClient;
class Model;
class ModuleDirectory;
class SecretStore;

// The modules' servers of the open workspace: one ApiClient for each profile
// and module with a ServerType, made on first use, sharing one
// QNetworkAccessManager. Each client queues at most four requests at once,
// keeps ETags, follows Link pages, waits on 429 and on GitHub's rate-limited
// 403, and pauses while its token
// is refused. See the M5 plan, part 3, and docs/integrations.md.
// Implemented in ServerClient.cpp.
class ServerClients : public QObject
{
    Q_OBJECT

public:
    // Without a network manager, makes its own. Tests pass one that answers
    // from a table. Everything passed must outlive this object.
    ServerClients(const Model &model, const ModuleDirectory &modules, SecretStore &secrets,
                  QNetworkAccessManager *network = nullptr, QObject *parent = nullptr);
    ~ServerClients() override;

    // The module's server in the profile, or null if the module registered
    // no ServerType. The client lives until clear() or this object goes.
    ApiClient *client(const QString &profileId, const QString &moduleId);

    // True while the client's token is refused. See failingChanged.
    bool failing(const QString &profileId, const QString &moduleId) const;
    // The failing servers, as (profile id, module id).
    QList<QPair<QString, QString>> failingServers() const;

    // Drops every client, for another workspace: the workspace id is part of
    // every token key. Requests in flight end without a callback.
    void clear();

signals:
    // A client started or stopped failing.
    void failingChanged(const QString &profileId, const QString &moduleId);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
