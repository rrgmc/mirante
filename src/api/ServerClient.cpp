#include "api/ServerClient.h"

#include "api/ModuleDirectory.h"
#include "api/SecretStore.h"
#include "core/Model.h"

#include "Version.h"

#include <mirante/ApiClient.h>
#include <mirante/ModuleSettings.h>

#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QRegularExpression>
#include <QTimer>

#include <deque>
#include <map>
#include <set>

namespace mirante {

namespace {

constexpr int kMaxRunning = 4;
constexpr int kMaxKeptUrls = 500;
constexpr int kMaxRetries = 3;
constexpr int kDefaultRetryAfterSeconds = 30;
constexpr int kTransferTimeoutMs = 30000;

// The "next" URL of a Link header, or an invalid URL.
QUrl nextLink(const QList<QPair<QByteArray, QByteArray>> &headers)
{
    static const QRegularExpression link(QStringLiteral(R"(<([^>]*)>\s*;([^,]*))"));
    static const QRegularExpression next(QStringLiteral(R"(rel\s*=\s*"?([^";]*\s)?next[\s";])"));
    for (const auto &header : headers) {
        if (header.first.compare("link", Qt::CaseInsensitive) != 0)
            continue;
        auto it = link.globalMatch(QString::fromUtf8(header.second));
        while (it.hasNext()) {
            const auto match = it.next();
            if (next.match(match.captured(2) + QLatin1Char(';')).hasMatch())
                return QUrl(match.captured(1));
        }
    }
    return {};
}

QByteArray headerValue(const QList<QPair<QByteArray, QByteArray>> &headers, const QByteArray &name)
{
    for (const auto &header : headers) {
        if (header.first.compare(name, Qt::CaseInsensitive) == 0)
            return header.second;
    }
    return {};
}

// Seconds to wait from a Retry-After header. Only the number form is read;
// an absent or other value waits the default.
int retryAfterSeconds(const QByteArray &value)
{
    bool ok = false;
    const int seconds = value.trimmed().toInt(&ok);
    return ok && seconds >= 0 ? seconds : kDefaultRetryAfterSeconds;
}

bool sameOrigin(const QUrl &a, const QUrl &b)
{
    return a.scheme() == b.scheme() && a.host().compare(b.host(), Qt::CaseInsensitive) == 0
           && a.port(a.scheme() == QLatin1String("http") ? 80 : 443)
                  == b.port(b.scheme() == QLatin1String("http") ? 80 : 443);
}

ApiReply errorReply(ApiReply::Error error, const QString &message, int httpStatus = 0)
{
    ApiReply reply;
    reply.error = error;
    reply.message = message;
    reply.httpStatus = httpStatus;
    return reply;
}

} // namespace

// One module's server in one profile. Owned by ServerClients.
class ServerClient : public QObject, public ApiClient
{
public:
    ServerClient(ServerClients &owner, const Model &model, const ServerType &type, const ModuleSettings *settings,
                 SecretStore &secrets, QNetworkAccessManager &network, QString profileId, QString moduleId);
    ~ServerClient() override;

    bool isFailing() const { return m_failing; }

    QUrl baseUrl() const override;
    void get(const QString &path, const QUrlQuery &query, const GetOptions &options, QObject *receiver,
             std::function<void(const ApiReply &)> done) override;
    void post(const QString &path, const QJsonDocument &body, QObject *receiver,
              std::function<void(const ApiReply &)> done) override;
    bool authorize(QNetworkRequest &request) override;
    void secret(QObject *receiver, std::function<void(std::optional<QString>)> done) override;

private:
    struct Request
    {
        bool isGet = true;
        QUrl url;   // the page being asked for
        QByteArray body;
        GetOptions options;
        QJsonObject values;
        bool hasReceiver = false;
        QPointer<QObject> receiver;
        std::function<void(const ApiReply &)> done;
        int retries = 0;
        QJsonArray rows;   // the pages so far, with followLinks
    };
    using RequestPtr = std::shared_ptr<Request>;

    struct Kept
    {
        QByteArray etag;
        QByteArray body;
        QList<QPair<QByteArray, QByteArray>> headers;
        quint64 used = 0;
    };

    QJsonObject values() const;
    QString tokenKey() const;
    QUrl urlFor(const QJsonObject &values, const QString &path) const;
    void enqueue(bool isGet, const QString &path, const QUrlQuery &query, const QByteArray &body,
                 const GetOptions &options, QObject *receiver, std::function<void(const ApiReply &)> done);
    void loadToken();
    void pump();
    void send(const RequestPtr &request);
    void finished(QNetworkReply *reply, const RequestPtr &request);
    void keep(const QString &url, const QByteArray &etag, const QByteArray &body,
              const QList<QPair<QByteArray, QByteArray>> &headers);
    void answer(const RequestPtr &request, const ApiReply &reply);
    void answerLater(const RequestPtr &request, const ApiReply &reply);
    void setFailing(bool failing);

    ServerClients *m_owner;
    const Model *m_model;
    const ServerType *m_type;
    const ModuleSettings *m_settings;
    SecretStore *m_secrets;
    QNetworkAccessManager *m_network;
    QString m_profileId;
    QString m_moduleId;

    // The token, once read. Never logged.
    bool m_tokenKnown = false;
    bool m_tokenLoading = false;
    std::optional<QString> m_token;

    std::deque<RequestPtr> m_queue;
    std::set<QNetworkReply *> m_running;
    QTimer m_retryTimer;   // running while a 429 holds the queue

    bool m_failing = false;
    QJsonObject m_failingValues;   // the screen values when the token was refused

    std::map<QString, Kept> m_kept;   // by full URL
    quint64 m_useCounter = 0;
};

ServerClient::ServerClient(ServerClients &owner, const Model &model, const ServerType &type,
                           const ModuleSettings *settings, SecretStore &secrets, QNetworkAccessManager &network,
                           QString profileId, QString moduleId)
    : m_owner(&owner)
    , m_model(&model)
    , m_type(&type)
    , m_settings(settings)
    , m_secrets(&secrets)
    , m_network(&network)
    , m_profileId(std::move(profileId))
    , m_moduleId(std::move(moduleId))
{
    m_retryTimer.setSingleShot(true);
    connect(&m_retryTimer, &QTimer::timeout, this, &ServerClient::pump);

    connect(m_secrets, &SecretStore::changed, this, [this](const QString &key) {
        if (key != tokenKey())
            return;
        m_tokenKnown = false;
        m_token.reset();
        setFailing(false);
        pump();
    });
    connect(m_model, &Model::profilesChanged, this, [this] {
        if (m_failing && values() != m_failingValues)
            setFailing(false);
    });
}

ServerClient::~ServerClient()
{
    // Requests in flight end without a callback.
    for (QNetworkReply *reply : m_running) {
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
    }
}

QJsonObject ServerClient::values() const
{
    return m_settings ? m_settings->values(m_profileId, m_type->screenId()) : QJsonObject();
}

QString ServerClient::tokenKey() const
{
    return secretKey(m_model->workspaceId(), m_profileId, m_moduleId, m_type->tokenKey());
}

QUrl ServerClient::baseUrl() const
{
    return m_type->baseUrl(values());
}

QUrl ServerClient::urlFor(const QJsonObject &values, const QString &path) const
{
    QUrl base = m_type->baseUrl(values);
    // Paths are relative to the base, which may sit below the host's root.
    QString basePath = base.path();
    if (!basePath.endsWith(QLatin1Char('/')))
        base.setPath(basePath + QLatin1Char('/'));
    QString relative = path;
    while (relative.startsWith(QLatin1Char('/')))
        relative.remove(0, 1);
    return base.resolved(QUrl(relative));
}

void ServerClient::get(const QString &path, const QUrlQuery &query, const GetOptions &options, QObject *receiver,
                       std::function<void(const ApiReply &)> done)
{
    enqueue(true, path, query, {}, options, receiver, std::move(done));
}

void ServerClient::post(const QString &path, const QJsonDocument &body, QObject *receiver,
                        std::function<void(const ApiReply &)> done)
{
    enqueue(false, path, {}, body.toJson(QJsonDocument::Compact), {}, receiver, std::move(done));
}

void ServerClient::enqueue(bool isGet, const QString &path, const QUrlQuery &query, const QByteArray &body,
                           const GetOptions &options, QObject *receiver, std::function<void(const ApiReply &)> done)
{
    auto request = std::make_shared<Request>();
    request->isGet = isGet;
    request->body = body;
    request->options = options;
    request->values = values();
    request->hasReceiver = receiver != nullptr;
    request->receiver = receiver;
    request->done = std::move(done);

    const QUrl base = m_type->baseUrl(request->values);
    const QString problem = m_type->problem(request->values);
    if (!base.isValid() || base.isEmpty()) {
        answerLater(request, errorReply(ApiReply::Error::NoServer, tr("No server set")));
        return;
    }
    if (!problem.isEmpty()) {
        answerLater(request, errorReply(ApiReply::Error::NoServer, problem));
        return;
    }
    if (m_failing) {
        answerLater(request, errorReply(ApiReply::Error::Paused, tr("Token refused")));
        return;
    }

    request->url = urlFor(request->values, path);
    if (!query.isEmpty())
        request->url.setQuery(query);
    m_queue.push_back(request);
    pump();
}

void ServerClient::loadToken()
{
    if (m_tokenKnown || m_tokenLoading)
        return;
    m_tokenLoading = true;
    m_secrets->read(tokenKey(), this, [this](std::optional<QString> token) {
        m_tokenLoading = false;
        m_tokenKnown = true;
        m_token.reset();
        if (token && !token->isEmpty())
            m_token = token;
        pump();
    });
}

void ServerClient::pump()
{
    if (m_queue.empty())
        return;
    if (m_failing) {
        while (!m_queue.empty()) {
            auto request = m_queue.front();
            m_queue.pop_front();
            answerLater(request, errorReply(ApiReply::Error::Paused, tr("Token refused")));
        }
        return;
    }
    if (!m_tokenKnown) {
        loadToken();
        return;
    }
    if (!m_token) {
        while (!m_queue.empty()) {
            auto request = m_queue.front();
            m_queue.pop_front();
            answerLater(request, errorReply(ApiReply::Error::NoToken, tr("Token not set on this device")));
        }
        return;
    }
    if (m_retryTimer.isActive())
        return;
    while (!m_queue.empty() && static_cast<int>(m_running.size()) < kMaxRunning) {
        auto request = m_queue.front();
        m_queue.pop_front();
        send(request);
    }
}

void ServerClient::send(const RequestPtr &request)
{
    QNetworkRequest network(request->url);
    network.setRawHeader("Accept", "application/json");
    network.setRawHeader("User-Agent", QByteArrayLiteral("Mirante/") + MIRANTE_VERSION);
    network.setTransferTimeout(kTransferTimeoutMs);
    m_type->authorize(network, *m_token, request->values);

    QNetworkReply *reply = nullptr;
    if (request->isGet) {
        const auto kept = m_kept.find(request->url.toString(QUrl::FullyEncoded));
        if (kept != m_kept.end() && !kept->second.etag.isEmpty())
            network.setRawHeader("If-None-Match", kept->second.etag);
        reply = m_network->get(network);
    } else {
        network.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/json"));
        reply = m_network->post(network, request->body);
    }
    m_running.insert(reply);
    connect(reply, &QNetworkReply::finished, this, [this, reply, request] { finished(reply, request); });
}

void ServerClient::finished(QNetworkReply *reply, const RequestPtr &request)
{
    m_running.erase(reply);
    reply->deleteLater();

    ApiReply result;
    result.httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    result.headers = reply->rawHeaderPairs();
    result.body = reply->readAll();
    const QString url = request->url.toString(QUrl::FullyEncoded);

    if (result.httpStatus == 0) {
        result.error = ApiReply::Error::Network;
        result.message = reply->error() == QNetworkReply::OperationCanceledError
                             ? tr("The server did not answer in time")
                             : reply->errorString();
        answer(request, result);
        pump();
        return;
    }

    if (result.httpStatus == 429) {
        if (request->retries < kMaxRetries) {
            ++request->retries;
            m_queue.push_front(request);
            m_retryTimer.start(retryAfterSeconds(headerValue(result.headers, "Retry-After")) * 1000);
            return;
        }
        result.error = ApiReply::Error::Http;
        result.message = tr("Too many requests (429)");
        answer(request, result);
        pump();
        return;
    }

    if (result.httpStatus == 304 && request->isGet) {
        const auto kept = m_kept.find(url);
        if (kept != m_kept.end()) {
            kept->second.used = ++m_useCounter;
            result.body = kept->second.body;
            result.headers = kept->second.headers;
        }
    } else if (result.httpStatus == 401 || (result.httpStatus == 403 && request->isGet)) {
        result.error = ApiReply::Error::Refused;
        result.message = tr("Token refused (%1)").arg(result.httpStatus);
        m_failingValues = request->values;
        setFailing(true);
        answer(request, result);
        pump();
        return;
    } else if (result.httpStatus == 403) {
        result.error = ApiReply::Error::Forbidden;
        result.message = tr("Not allowed (403)");
    } else if (result.httpStatus == 404) {
        result.error = ApiReply::Error::NotFound;
        result.message = tr("Not found (404)");
    } else if (result.httpStatus >= 400) {
        result.error = ApiReply::Error::Http;
        const QString reason = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
        result.message = reason.isEmpty() ? tr("Server error (%1)").arg(result.httpStatus)
                                          : QStringLiteral("%1 (%2)").arg(reason).arg(result.httpStatus);
    } else if (request->isGet) {
        const QByteArray etag = headerValue(result.headers, "ETag");
        if (!etag.isEmpty())
            keep(url, etag, result.body, result.headers);
    }

    if (result.ok())
        result.json = QJsonDocument::fromJson(result.body);

    if (result.ok() && request->isGet && request->options.followLinks && result.json.isArray()) {
        const QJsonArray page = result.json.array();
        for (const QJsonValue &row : page) {
            if (request->rows.size() >= request->options.maxRows)
                break;
            request->rows.append(row);
        }
        const QUrl next = request->url.resolved(nextLink(result.headers));
        // The token goes only to the server it belongs to.
        if (request->rows.size() < request->options.maxRows && !page.isEmpty() && next.isValid()
            && next != request->url && sameOrigin(next, request->url)) {
            request->url = next;
            m_queue.push_front(request);
            pump();
            return;
        }
        result.json = QJsonDocument(request->rows);
        result.body = result.json.toJson(QJsonDocument::Compact);
    }

    answer(request, result);
    pump();
}

void ServerClient::keep(const QString &url, const QByteArray &etag, const QByteArray &body,
                        const QList<QPair<QByteArray, QByteArray>> &headers)
{
    m_kept[url] = Kept{etag, body, headers, ++m_useCounter};
    if (static_cast<int>(m_kept.size()) <= kMaxKeptUrls)
        return;
    auto oldest = m_kept.begin();
    for (auto it = m_kept.begin(); it != m_kept.end(); ++it) {
        if (it->second.used < oldest->second.used)
            oldest = it;
    }
    m_kept.erase(oldest);
}

void ServerClient::answer(const RequestPtr &request, const ApiReply &reply)
{
    if (request->hasReceiver && !request->receiver)
        return;
    if (request->done)
        request->done(reply);
}

void ServerClient::answerLater(const RequestPtr &request, const ApiReply &reply)
{
    // Every answer is asynchronous, even one known at once.
    QMetaObject::invokeMethod(this, [this, request, reply] { answer(request, reply); }, Qt::QueuedConnection);
}

void ServerClient::setFailing(bool failing)
{
    if (m_failing == failing)
        return;
    m_failing = failing;
    emit m_owner->failingChanged(m_profileId, m_moduleId);
}

bool ServerClient::authorize(QNetworkRequest &request)
{
    if (!m_tokenKnown) {
        loadToken();
        return false;
    }
    if (!m_token)
        return false;
    m_type->authorize(request, *m_token, values());
    return true;
}

void ServerClient::secret(QObject *receiver, std::function<void(std::optional<QString>)> done)
{
    m_secrets->read(tokenKey(), receiver, std::move(done));
}

struct ServerClients::Private
{
    const Model *model = nullptr;
    const ModuleDirectory *modules = nullptr;
    SecretStore *secrets = nullptr;
    QNetworkAccessManager *network = nullptr;
    std::map<QPair<QString, QString>, std::unique_ptr<ServerClient>> clients;
};

ServerClients::ServerClients(const Model &model, const ModuleDirectory &modules, SecretStore &secrets,
                             QNetworkAccessManager *network, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
    d->model = &model;
    d->modules = &modules;
    d->secrets = &secrets;
    d->network = network ? network : new QNetworkAccessManager(this);
}

ServerClients::~ServerClients() = default;

ApiClient *ServerClients::client(const QString &profileId, const QString &moduleId)
{
    const QPair<QString, QString> key(profileId, moduleId);
    auto &slot = d->clients[key];
    if (!slot) {
        const ServerType *type = d->modules->serverType(moduleId);
        if (!type) {
            d->clients.erase(key);
            return nullptr;
        }
        slot = std::make_unique<ServerClient>(*this, *d->model, *type, d->modules->moduleSettings(moduleId),
                                              *d->secrets, *d->network, profileId, moduleId);
    }
    return slot.get();
}

bool ServerClients::failing(const QString &profileId, const QString &moduleId) const
{
    const auto it = d->clients.find(qMakePair(profileId, moduleId));
    return it != d->clients.end() && it->second->isFailing();
}

QList<QPair<QString, QString>> ServerClients::failingServers() const
{
    QList<QPair<QString, QString>> result;
    for (const auto &[key, client] : d->clients) {
        if (client->isFailing())
            result.append(key);
    }
    return result;
}

void ServerClients::clear()
{
    const QList<QPair<QString, QString>> wereFailing = failingServers();
    d->clients.clear();
    for (const auto &server : wereFailing)
        emit failingChanged(server.first, server.second);
}

} // namespace mirante
