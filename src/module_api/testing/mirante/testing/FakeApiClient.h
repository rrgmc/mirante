#pragma once

#include <mirante/ApiClient.h>

#include <QCoreApplication>
#include <QFile>
#include <QJsonParseError>
#include <QMap>
#include <QMetaObject>
#include <QNetworkRequest>
#include <QObject>
#include <QPointer>
#include <QStringList>

namespace mirante::testing {

// An ApiClient for module tests. It answers each request from a table of
// replies, keyed by the method, the path and the query, such as
// "GET api/v4/projects/a%2Fb/merge_requests/1" or "GET api/v4/user?x=1", and
// records every request. Unknown requests get a 404. Answers are queued, as a
// real client's are, so a test waits with QTRY_VERIFY or calls
// QCoreApplication::processEvents(); setSynchronous(true) answers at once.
class FakeApiClient : public ApiClient
{
public:
    struct Request
    {
        QString method;     // "GET", "POST" or "PUT"
        QString path;
        QUrlQuery query;
        GetOptions options;
        QJsonDocument body;
    };

    explicit FakeApiClient(QUrl baseUrl = QUrl(QStringLiteral("https://server.example")))
        : m_baseUrl(std::move(baseUrl))
    {
    }

    // The key a request is looked up by: "<method> <path>", then "?" and the
    // query if it has one.
    static QString key(const QString &method, const QString &path, const QUrlQuery &query = {})
    {
        const QString text = query.toString(QUrl::FullyEncoded);
        return method + QLatin1Char(' ') + path + (text.isEmpty() ? QString() : QLatin1Char('?') + text);
    }

    void setReply(const QString &key, const ApiReply &reply) { m_replies.insert(key, reply); }

    // A 200 reply with this JSON.
    void setJson(const QString &key, const QJsonDocument &json)
    {
        ApiReply reply;
        reply.httpStatus = 200;
        reply.json = json;
        reply.body = json.toJson(QJsonDocument::Compact);
        setReply(key, reply);
    }

    // A 200 reply with the JSON in a file, such as a recorded response in the
    // module's tests/data folder. Fails the lookup, with a 404, if the file
    // cannot be read or parsed.
    bool setJsonFile(const QString &key, const QString &fileName)
    {
        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly))
            return false;
        QJsonParseError error;
        const QJsonDocument json = QJsonDocument::fromJson(file.readAll(), &error);
        if (error.error != QJsonParseError::NoError)
            return false;
        setJson(key, json);
        return true;
    }

    // An error reply, such as Error::NoToken, for every request.
    void failAll(ApiReply::Error error, const QString &message = {})
    {
        m_failAll = error;
        m_failMessage = message;
    }

    void setSynchronous(bool on) { m_synchronous = on; }
    void setToken(std::optional<QString> token) { m_token = std::move(token); }

    const QList<Request> &requests() const { return m_requests; }
    QStringList requestKeys() const
    {
        QStringList keys;
        for (const Request &request : m_requests)
            keys << key(request.method, request.path, request.query);
        return keys;
    }

    QUrl baseUrl() const override { return m_baseUrl; }

    void get(const QString &path, const QUrlQuery &query, const GetOptions &options, QObject *receiver,
             std::function<void(const ApiReply &)> done) override
    {
        m_requests.append({QStringLiteral("GET"), path, query, options, {}});
        answer(key(QStringLiteral("GET"), path, query), receiver, std::move(done));
    }

    void post(const QString &path, const QJsonDocument &body, QObject *receiver,
              std::function<void(const ApiReply &)> done) override
    {
        m_requests.append({QStringLiteral("POST"), path, {}, {}, body});
        answer(key(QStringLiteral("POST"), path), receiver, std::move(done));
    }

    void put(const QString &path, const QJsonDocument &body, QObject *receiver,
             std::function<void(const ApiReply &)> done) override
    {
        m_requests.append({QStringLiteral("PUT"), path, {}, {}, body});
        answer(key(QStringLiteral("PUT"), path), receiver, std::move(done));
    }

    bool authorize(QNetworkRequest &request) override
    {
        if (!m_token)
            return false;
        request.setRawHeader("X-Fake-Token", m_token->toUtf8());
        return true;
    }

    void secret(QObject *receiver, std::function<void(std::optional<QString>)> done) override
    {
        const std::optional<QString> token = m_token;
        deliver(receiver, [done = std::move(done), token] { done(token); });
    }

private:
    void answer(const QString &requestKey, QObject *receiver, std::function<void(const ApiReply &)> done)
    {
        ApiReply reply;
        if (m_failAll != ApiReply::Error::None) {
            reply.error = m_failAll;
            reply.message = m_failMessage;
        } else if (m_replies.contains(requestKey)) {
            reply = m_replies.value(requestKey);
        } else {
            reply.error = ApiReply::Error::NotFound;
            reply.httpStatus = 404;
            reply.message = QStringLiteral("Not found (404)");
        }
        deliver(receiver, [done = std::move(done), reply] { done(reply); });
    }

    void deliver(QObject *receiver, std::function<void()> call)
    {
        if (m_synchronous) {
            call();
            return;
        }
        QObject *context = receiver ? receiver : QCoreApplication::instance();
        QMetaObject::invokeMethod(context, std::move(call), Qt::QueuedConnection);
    }

    QUrl m_baseUrl;
    QMap<QString, ApiReply> m_replies;
    QList<Request> m_requests;
    ApiReply::Error m_failAll = ApiReply::Error::None;
    QString m_failMessage;
    std::optional<QString> m_token = QStringLiteral("fake-token");
    bool m_synchronous = false;
};

} // namespace mirante::testing
