#pragma once

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QPair>
#include <QString>
#include <QUrl>
#include <QUrlQuery>

#include <functional>
#include <optional>

class QNetworkRequest;
class QObject;

namespace mirante {

// How a module's server is set up in a profile: which settings screen holds
// it, which of its fields is the token, and how a request is authenticated.
// A module registers one with ModuleRegistrar::setServerType(). The core
// never names a service; this is all it knows about one. See
// docs/modules.md#servers-and-tokens.
class ServerType
{
public:
    virtual ~ServerType() = default;

    // The settings screen that holds the server, such as "general", and the
    // key of its Secret field, such as "token".
    virtual QString screenId() const = 0;
    virtual QString tokenKey() const = 0;

    // The base URL in a profile's screen values, or an invalid URL if none
    // is set.
    virtual QUrl baseUrl(const QJsonObject &values) const = 0;

    // Why the values cannot be used even with a token, such as Jira Cloud
    // without an email, or empty if they can. Shown to the user.
    virtual QString problem(const QJsonObject &values) const
    {
        Q_UNUSED(values)
        return {};
    }

    // Adds the authentication to a request, such as a PRIVATE-TOKEN header.
    virtual void authorize(QNetworkRequest &request, const QString &token, const QJsonObject &values) const = 0;
};

// The answer to one request of an ApiClient.
struct ApiReply
{
    enum class Error {
        None,
        NoServer,   // no base URL in the profile, or ServerType::problem()
        NoToken,    // no token on this device
        Refused,    // 401, or 403 on a GET: the server is now failing
        Forbidden,  // 403 on a POST
        NotFound,   // 404
        Http,       // any other status of 400 or more
        Network,    // no answer: DNS, TLS, timeout, offline
        Paused,     // the server is failing; nothing was sent
    };

    Error error = Error::None;
    int httpStatus = 0;
    QJsonDocument json;         // the body, parsed; the cached one after a 304
    QByteArray body;
    QList<QPair<QByteArray, QByteArray>> headers;
    QString message;            // for the user, such as "Not found (404)"

    bool ok() const { return error == Error::None; }
};

struct GetOptions
{
    // Follow the Link header's "next" page and join the JSON arrays, up to
    // maxRows rows. GitLab lists use it. Jira pages itself.
    bool followLinks = false;
    int maxRows = 100;
};

// A module's server in one profile. Every request is asynchronous and runs on
// the main thread. A callback is dropped if its receiver is destroyed first.
// See docs/integrations.md.
class ApiClient
{
public:
    virtual ~ApiClient() = default;

    // The base URL, or invalid. Paths are relative to it, so a server
    // installed at a path works: "api/v4/user", not "/api/v4/user".
    virtual QUrl baseUrl() const = 0;

    // A GET sends If-None-Match with the last ETag of the same URL, and a 304
    // answers with the body kept from before.
    virtual void get(const QString &path, const QUrlQuery &query, const GetOptions &options, QObject *receiver,
                     std::function<void(const ApiReply &)> done) = 0;
    virtual void post(const QString &path, const QJsonDocument &body, QObject *receiver,
                      std::function<void(const ApiReply &)> done) = 0;

    // Adds the authentication to a request the module makes itself. False if
    // the token is not loaded yet or not set. Prefer get() and post().
    virtual bool authorize(QNetworkRequest &request) = 0;

    // The raw token, loaded from the keychain if needed. Use it only where a
    // request is not enough, such as passing the token to a command. Each
    // call needs a comment that says why authorize() does not work there.
    virtual void secret(QObject *receiver, std::function<void(std::optional<QString>)> done) = 0;
};

} // namespace mirante
