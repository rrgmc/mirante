// Tests ServerClients and its ApiClients against a QNetworkAccessManager that
// answers from the test instead of the network: joining paths to the base
// URL, the errors given without a request, the limit of four requests, ETags,
// Link paging, 429 and rate-limited 403 retries, the failing state and
// dropped callbacks.
// No test uses the network or the keychain.
#include "api/ModuleDirectory.h"
#include "api/SecretBackend.h"
#include "api/SecretStore.h"
#include "api/ServerClient.h"
#include "core/Model.h"

#include <mirante/ApiClient.h>
#include <mirante/ModuleSettings.h>

#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>

#include <cstring>
#include <functional>
#include <map>
#include <memory>

using namespace mirante;

namespace {

const QString kWorkspace = QStringLiteral("ws");
const QString kProfile = QStringLiteral("default");
const QString kModule = QStringLiteral("srv");

// A reply whose answer the test gives, at once or later.
class FakeReply : public QNetworkReply
{
public:
    FakeReply(QNetworkAccessManager::Operation op, const QNetworkRequest &request, QObject *parent)
        : QNetworkReply(parent)
    {
        setOperation(op);
        setRequest(request);
        setUrl(request.url());
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
    }

    // An HTTP answer, or with status 0 a network error.
    void respond(int status, const QByteArray &body = {}, const QList<QPair<QByteArray, QByteArray>> &headers = {})
    {
        if (isFinished())
            return;
        m_data = body;
        if (status > 0) {
            setAttribute(QNetworkRequest::HttpStatusCodeAttribute, status);
            for (const auto &header : headers)
                setRawHeader(header.first, header.second);
            if (status >= 400)
                setError(QNetworkReply::ProtocolInvalidOperationError, QStringLiteral("HTTP %1").arg(status));
        } else {
            setError(QNetworkReply::HostNotFoundError, QStringLiteral("Host not found"));
            emit errorOccurred(error());
        }
        setFinished(true);
        emit finished();
    }

    void abort() override
    {
        if (isFinished())
            return;
        setError(QNetworkReply::OperationCanceledError, QStringLiteral("Aborted"));
        setFinished(true);
        emit finished();
    }

    bool isSequential() const override { return true; }
    qint64 bytesAvailable() const override { return m_data.size() - m_pos + QIODevice::bytesAvailable(); }

protected:
    qint64 readData(char *data, qint64 maxSize) override
    {
        const qint64 count = qMin(maxSize, qint64(m_data.size() - m_pos));
        memcpy(data, m_data.constData() + m_pos, size_t(count));
        m_pos += count;
        return count;
    }

private:
    QByteArray m_data;
    qint64 m_pos = 0;
};

struct Answer
{
    int status = 200;
    QByteArray body;
    QList<QPair<QByteArray, QByteArray>> headers;
};

// Records every request. With a handler, answers each one in the next event
// loop pass; without one, the replies wait until the test answers them.
class FakeNetwork : public QNetworkAccessManager
{
public:
    struct Sent
    {
        Operation op;
        QNetworkRequest request;
        QByteArray body;
        QPointer<FakeReply> reply;
    };

    std::function<Answer(const Sent &)> handler;
    QList<Sent> sent;

    QString url(int i) const { return sent.at(i).request.url().toString(QUrl::FullyEncoded); }

protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &request, QIODevice *outgoing) override
    {
        auto *reply = new FakeReply(op, request, this);
        sent.append({op, request, outgoing ? outgoing->readAll() : QByteArray(), reply});
        if (handler) {
            const Answer answer = handler(sent.last());
            QTimer::singleShot(0, reply, [reply, answer] { reply->respond(answer.status, answer.body, answer.headers); });
        }
        return reply;
    }
};

class TestServerType : public ServerType
{
public:
    QString screenId() const override { return QStringLiteral("general"); }
    QString tokenKey() const override { return QStringLiteral("token"); }
    QUrl baseUrl(const QJsonObject &values) const override { return QUrl(values.value(QStringLiteral("baseUrl")).toString()); }
    QString problem(const QJsonObject &values) const override { return values.value(QStringLiteral("problem")).toString(); }
    void authorize(QNetworkRequest &request, const QString &token, const QJsonObject &values) const override
    {
        Q_UNUSED(values)
        request.setRawHeader("X-Token", token.toUtf8());
    }
};

class TestSettings : public ModuleSettings
{
public:
    QJsonObject screen;
    QJsonObject values(const QString &profileId, const QString &screenId) const override
    {
        return profileId == kProfile && screenId == QLatin1String("general") ? screen : QJsonObject();
    }
};

class TestDirectory : public ModuleDirectory
{
public:
    TestServerType type;
    TestSettings settings;

    QStringList moduleIds() const override { return {kModule, QStringLiteral("plain")}; }
    const ItemType *findType(const QString &) const override { return nullptr; }
    const ServerType *serverType(const QString &moduleId) const override { return moduleId == kModule ? &type : nullptr; }
    const ModuleSettings *moduleSettings(const QString &moduleId) const override
    {
        return moduleId == kModule ? &settings : nullptr;
    }
    std::vector<const SettingsScreen *> settingsScreens(const QString &) const override { return {}; }
};

QByteArray header(const FakeNetwork::Sent &sent, const QByteArray &name)
{
    return sent.request.rawHeader(name);
}

QJsonArray numbers(int from, int count)
{
    QJsonArray array;
    for (int i = 0; i < count; ++i)
        array.append(from + i);
    return array;
}

QByteArray json(const QJsonArray &array)
{
    return QJsonDocument(array).toJson(QJsonDocument::Compact);
}

// A GitHub search page: {"total_count": total, "items": rows}.
QByteArray searchPage(int total, const QJsonArray &rows)
{
    return QJsonDocument(QJsonObject{{QStringLiteral("total_count"), total}, {QStringLiteral("items"), rows}})
        .toJson(QJsonDocument::Compact);
}

} // namespace

class TestServerClient : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void clientOnlyForServerModules();
    void pathJoinsBaseWithPath();
    void headersAndToken();
    void tokenIsTrimmed();
    void noServerSendsNothing();
    void problemSendsNothing();
    void noTokenSendsNothing();
    void fourAtATime();
    void etagServesKeptBody();
    void pagingJoinsArrays();
    void pagingStopsAtMaxRows();
    void pagingStaysOnTheServer();
    void pagingJoinsObjects();
    void pagingObjectWithoutKey();
    void pagingObjectsFromKeptPages();
    void retriesAfter429();
    void otherRequestsWaitOn429();
    void givesUpAfterThree429();
    void rateLimited403WaitsForReset();
    void rateLimited403WaitsForRetryAfter();
    void givesUpAfterThreeRateLimited403();
    void refusedPausesUntilTokenChanges();
    void settingsChangeResumes();
    void forbiddenGetPauses();
    void forbiddenPostDoesNotPause();
    void putSendsMethodAndBody();
    void putSendsNoEtag();
    void forbiddenPutDoesNotPause();
    void errorsMapped();
    void destroyedReceiverGetsNoCallback();
    void queuedGetOfDestroyedReceiverIsNotSent();
    void clearDropsRequestsInFlight();

private:
    ApiClient *client() { return m_clients->client(kProfile, kModule); }
    QString key() const { return secretKey(kWorkspace, kProfile, kModule, QStringLiteral("token")); }
    // Sends a GET and waits for its answer.
    ApiReply getAndWait(const QString &path, const GetOptions &options = {}, const QUrlQuery &query = {});
    void setToken(const QString &token);

    std::unique_ptr<Model> m_model;
    std::unique_ptr<TestDirectory> m_directory;
    std::unique_ptr<SecretStore> m_secrets;
    std::unique_ptr<FakeNetwork> m_network;
    std::unique_ptr<ServerClients> m_clients;
};

void TestServerClient::init()
{
    m_model = std::make_unique<Model>();
    m_model->setWorkspaceId(kWorkspace);
    m_directory = std::make_unique<TestDirectory>();
    m_directory->settings.screen = {{QStringLiteral("baseUrl"), QStringLiteral("https://git.example/")}};
    m_secrets = std::make_unique<SecretStore>(makeMemoryBackend());
    m_network = std::make_unique<FakeNetwork>();
    m_clients = std::make_unique<ServerClients>(*m_model, *m_directory, *m_secrets, m_network.get());

    setToken(QStringLiteral("secret-1"));
    // Until task T1 lands, SecretStore is a stub that never answers a read,
    // so nothing can be sent. The tests that need no token still run.
    const QByteArray test = QTest::currentTestFunction();
    if (test != "clientOnlyForServerModules" && test != "noServerSendsNothing" && test != "problemSendsNothing"
        && m_secrets->state(key()) != SecretStore::State::Set)
        QSKIP("SecretStore is still a stub: it keeps no token.");
}

void TestServerClient::cleanup()
{
    m_clients.reset();
    m_network.reset();
    m_secrets.reset();
    m_directory.reset();
    m_model.reset();
}

void TestServerClient::setToken(const QString &token)
{
    m_secrets->write(key(), token);
    // The memory backend answers at once; T1 may answer in a later pass.
    (void)QTest::qWaitFor([&] { return m_secrets->state(key()) == SecretStore::State::Set; }, 500);
}

ApiReply TestServerClient::getAndWait(const QString &path, const GetOptions &options, const QUrlQuery &query)
{
    ApiReply result;
    bool answered = false;
    client()->get(path, query, options, this, [&](const ApiReply &reply) {
        result = reply;
        answered = true;
    });
    if (!QTest::qWaitFor([&] { return answered; }, 5000))
        qWarning("No answer");
    return result;
}

void TestServerClient::clientOnlyForServerModules()
{
    QVERIFY(client());
    QCOMPARE(client(), m_clients->client(kProfile, kModule));
    QVERIFY(!m_clients->client(kProfile, QStringLiteral("plain")));
    QCOMPARE(client()->baseUrl(), QUrl(QStringLiteral("https://git.example/")));
}

void TestServerClient::pathJoinsBaseWithPath()
{
    m_network->handler = [](const FakeNetwork::Sent &) { return Answer{200, "{}", {}}; };
    m_directory->settings.screen[QStringLiteral("baseUrl")] = QStringLiteral("https://host.example/gitlab");
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("state"), QStringLiteral("opened"));
    // Modules encode a branch name themselves, as GitLab's ref does.
    query.addQueryItem(QStringLiteral("ref"), QString::fromLatin1(QUrl::toPercentEncoding(QStringLiteral("a+b&c"))));
    QVERIFY(getAndWait(QStringLiteral("api/v4/projects/a%2Fb/merge_requests"), {}, query).ok());
    m_directory->settings.screen[QStringLiteral("baseUrl")] = QStringLiteral("https://host.example/gitlab/");
    QVERIFY(getAndWait(QStringLiteral("/api/v4/user")).ok());

    QCOMPARE(m_network->sent.size(), 2);
    QCOMPARE(m_network->url(0),
             QStringLiteral("https://host.example/gitlab/api/v4/projects/a%2Fb/merge_requests?state=opened&ref=a%2Bb%26c"));
    QCOMPARE(m_network->url(1), QStringLiteral("https://host.example/gitlab/api/v4/user"));
}

void TestServerClient::headersAndToken()
{
    m_network->handler = [](const FakeNetwork::Sent &) { return Answer{201, R"({"id":1})", {}}; };
    bool answered = false;
    ApiReply result;
    client()->post(QStringLiteral("api/v4/things"), QJsonDocument(QJsonObject{{QStringLiteral("a"), 1}}), this,
                   [&](const ApiReply &reply) {
                       result = reply;
                       answered = true;
                   });
    QTRY_VERIFY(answered);
    QVERIFY(result.ok());
    QCOMPARE(result.json.object().value(QStringLiteral("id")).toInt(), 1);

    const auto &sent = m_network->sent.at(0);
    QCOMPARE(sent.op, QNetworkAccessManager::PostOperation);
    QCOMPARE(sent.body, QByteArray(R"({"a":1})"));
    QCOMPARE(header(sent, "X-Token"), QByteArray("secret-1"));
    QCOMPARE(header(sent, "Accept"), QByteArray("application/json"));
    QVERIFY(header(sent, "User-Agent").startsWith("Mirante/"));
    QCOMPARE(sent.request.transferTimeout(), 30000);

    QNetworkRequest own;
    QVERIFY(client()->authorize(own));
    QCOMPARE(own.rawHeader("X-Token"), QByteArray("secret-1"));
}

void TestServerClient::tokenIsTrimmed()
{
    // As stored by a paste that brought a line break along.
    setToken(QStringLiteral(" secret-2\r\n"));
    m_network->handler = [](const FakeNetwork::Sent &) { return Answer{200, "{}", {}}; };
    QVERIFY(getAndWait(QStringLiteral("api/v4/user")).ok());
    QCOMPARE(header(m_network->sent.last(), "X-Token"), QByteArray("secret-2"));
}

void TestServerClient::noServerSendsNothing()
{
    m_directory->settings.screen = {};
    const ApiReply reply = getAndWait(QStringLiteral("api/v4/user"));
    QCOMPARE(reply.error, ApiReply::Error::NoServer);
    QVERIFY(m_network->sent.isEmpty());
}

void TestServerClient::problemSendsNothing()
{
    m_directory->settings.screen[QStringLiteral("problem")] = QStringLiteral("Set the email");
    const ApiReply reply = getAndWait(QStringLiteral("api/v4/user"));
    QCOMPARE(reply.error, ApiReply::Error::NoServer);
    QCOMPARE(reply.message, QStringLiteral("Set the email"));
    QVERIFY(m_network->sent.isEmpty());
}

void TestServerClient::noTokenSendsNothing()
{
    m_secrets->remove(key());
    QTRY_COMPARE(m_secrets->state(key()), SecretStore::State::NotSet);
    const ApiReply reply = getAndWait(QStringLiteral("api/v4/user"));
    QCOMPARE(reply.error, ApiReply::Error::NoToken);
    QVERIFY(m_network->sent.isEmpty());
    QNetworkRequest own;
    QVERIFY(!client()->authorize(own));
}

void TestServerClient::fourAtATime()
{
    QStringList answered;
    for (int i = 0; i < 6; ++i) {
        client()->get(QStringLiteral("r%1").arg(i), {}, {}, this,
                      [&answered, i](const ApiReply &) { answered << QString::number(i); });
    }
    QTRY_COMPARE(m_network->sent.size(), 4);
    QTest::qWait(20);
    QCOMPARE(m_network->sent.size(), 4);

    m_network->sent.at(1).reply->respond(200, "[]");
    QTRY_COMPARE(m_network->sent.size(), 5);
    QCOMPARE(m_network->url(4), QStringLiteral("https://git.example/r4"));
    for (int i = 0; i < m_network->sent.size(); ++i)
        m_network->sent.at(i).reply->respond(200, "[]");
    QTRY_COMPARE(m_network->sent.size(), 6);
    m_network->sent.at(5).reply->respond(200, "[]");
    QTRY_COMPARE(answered.size(), 6);
    QCOMPARE(answered.first(), QStringLiteral("1"));
}

void TestServerClient::etagServesKeptBody()
{
    m_network->handler = [](const FakeNetwork::Sent &sent) {
        if (sent.request.rawHeader("If-None-Match") == "\"v1\"")
            return Answer{304, {}, {}};
        return Answer{200, "[1,2]", {{"ETag", "\"v1\""}}};
    };
    ApiReply first = getAndWait(QStringLiteral("list"));
    QVERIFY(first.ok());
    QVERIFY(header(m_network->sent.at(0), "If-None-Match").isEmpty());

    ApiReply second = getAndWait(QStringLiteral("list"));
    QVERIFY(second.ok());
    QCOMPARE(second.httpStatus, 304);
    QCOMPARE(header(m_network->sent.at(1), "If-None-Match"), QByteArray("\"v1\""));
    QCOMPARE(second.json, first.json);
    QCOMPARE(second.json.array().size(), 2);

    // Another URL has its own ETag.
    getAndWait(QStringLiteral("list"), {}, QUrlQuery(QStringLiteral("page=2")));
    QVERIFY(header(m_network->sent.at(2), "If-None-Match").isEmpty());
}

void TestServerClient::pagingJoinsArrays()
{
    m_network->handler = [](const FakeNetwork::Sent &sent) {
        const QString page = QUrlQuery(sent.request.url()).queryItemValue(QStringLiteral("page"));
        if (page == QLatin1String("2"))
            return Answer{200, json(numbers(3, 3)),
                          {{"Link", R"(<https://git.example/items?page=3>; rel="next", <https://git.example/items?page=1>; rel="first")"}}};
        if (page == QLatin1String("3"))
            return Answer{200, json(numbers(6, 1)), {}};
        return Answer{200, json(numbers(0, 3)), {{"Link", R"(<https://git.example/items?page=2>; rel="next")"}}};
    };
    GetOptions options;
    options.followLinks = true;
    const ApiReply reply = getAndWait(QStringLiteral("items"), options);
    QVERIFY(reply.ok());
    QCOMPARE(reply.json.array(), numbers(0, 7));
    QCOMPARE(m_network->sent.size(), 3);
}

void TestServerClient::pagingStopsAtMaxRows()
{
    m_network->handler = [](const FakeNetwork::Sent &sent) {
        const int page = QUrlQuery(sent.request.url()).queryItemValue(QStringLiteral("page")).toInt();
        const QByteArray next = "<https://git.example/items?page=" + QByteArray::number(page + 1) + R"(>; rel="next")";
        return Answer{200, json(numbers(page * 10, 10)), {{"Link", next}}};
    };
    GetOptions options;
    options.followLinks = true;
    options.maxRows = 25;
    const ApiReply reply = getAndWait(QStringLiteral("items?page=0"), options);
    QVERIFY(reply.ok());
    QCOMPARE(reply.json.array(), numbers(0, 25));
    QCOMPARE(m_network->sent.size(), 3);
}

void TestServerClient::pagingStaysOnTheServer()
{
    m_network->handler = [](const FakeNetwork::Sent &) {
        return Answer{200, "[1]", {{"Link", R"(<https://elsewhere.example/items?page=2>; rel="next")"}}};
    };
    GetOptions options;
    options.followLinks = true;
    const ApiReply reply = getAndWait(QStringLiteral("items"), options);
    QVERIFY(reply.ok());
    QCOMPARE(reply.json.array().size(), 1);
    QCOMPARE(m_network->sent.size(), 1);
}

void TestServerClient::pagingJoinsObjects()
{
    m_network->handler = [](const FakeNetwork::Sent &sent) {
        const int page = QUrlQuery(sent.request.url()).queryItemValue(QStringLiteral("page")).toInt();
        const QByteArray next = "<https://git.example/search?page=" + QByteArray::number(page + 1) + R"(>; rel="next")";
        // Only the first page's total is kept.
        return Answer{200, searchPage(page == 0 ? 100 : -1, numbers(page * 10, 10)), {{"Link", next}}};
    };
    GetOptions options;
    options.followLinks = true;
    options.maxRows = 25;
    options.arrayKey = QStringLiteral("items");
    const ApiReply reply = getAndWait(QStringLiteral("search?page=0"), options);
    QVERIFY(reply.ok());
    QVERIFY(reply.json.isObject());
    QCOMPARE(reply.json.object().value(QStringLiteral("total_count")).toInt(), 100);
    QCOMPARE(reply.json.object().value(QStringLiteral("items")).toArray(), numbers(0, 25));
    QCOMPARE(QJsonDocument::fromJson(reply.body), reply.json);
    QCOMPARE(m_network->sent.size(), 3);
}

void TestServerClient::pagingObjectWithoutKey()
{
    m_network->handler = [](const FakeNetwork::Sent &) {
        return Answer{200, R"({"message":"no rows"})", {{"Link", R"(<https://git.example/search?page=2>; rel="next")"}}};
    };
    GetOptions options;
    options.followLinks = true;
    options.arrayKey = QStringLiteral("items");
    const ApiReply reply = getAndWait(QStringLiteral("search"), options);
    QVERIFY(reply.ok());
    QCOMPARE(reply.json.object(), (QJsonObject{{QStringLiteral("message"), QStringLiteral("no rows")}}));
    QCOMPARE(m_network->sent.size(), 1);
}

void TestServerClient::pagingObjectsFromKeptPages()
{
    m_network->handler = [](const FakeNetwork::Sent &sent) {
        const QString page = QUrlQuery(sent.request.url()).queryItemValue(QStringLiteral("page"));
        if (!sent.request.rawHeader("If-None-Match").isEmpty())
            return Answer{304, {}, {}};
        if (page == QLatin1String("2"))
            return Answer{200, searchPage(-1, numbers(3, 2)), {{"ETag", "\"p2\""}}};
        return Answer{200, searchPage(5, numbers(0, 3)),
                      {{"ETag", "\"p1\""}, {"Link", R"(<https://git.example/search?page=2>; rel="next")"}}};
    };
    GetOptions options;
    options.followLinks = true;
    options.arrayKey = QStringLiteral("items");
    const ApiReply first = getAndWait(QStringLiteral("search"), options);
    QVERIFY(first.ok());
    QCOMPARE(first.json.object().value(QStringLiteral("items")).toArray(), numbers(0, 5));

    // Both pages answer 304: their kept bodies take their place.
    const ApiReply second = getAndWait(QStringLiteral("search"), options);
    QVERIFY(second.ok());
    QCOMPARE(m_network->sent.size(), 4);
    QCOMPARE(header(m_network->sent.at(3), "If-None-Match"), QByteArray("\"p2\""));
    QCOMPARE(second.json, first.json);
    QCOMPARE(second.json.object().value(QStringLiteral("total_count")).toInt(), 5);
}

void TestServerClient::retriesAfter429()
{
    int calls = 0;
    m_network->handler = [&calls](const FakeNetwork::Sent &) {
        return ++calls == 1 ? Answer{429, {}, {{"Retry-After", "0"}}} : Answer{200, "[]", {}};
    };
    const ApiReply reply = getAndWait(QStringLiteral("r"));
    QVERIFY(reply.ok());
    QCOMPARE(m_network->sent.size(), 2);
}

void TestServerClient::otherRequestsWaitOn429()
{
    int calls = 0;
    m_network->handler = [&calls](const FakeNetwork::Sent &) {
        return ++calls == 1 ? Answer{429, {}, {{"Retry-After", "1"}}} : Answer{200, "[]", {}};
    };
    int answered = 0;
    client()->get(QStringLiteral("a"), {}, {}, this, [&](const ApiReply &) { ++answered; });
    QTRY_COMPARE(m_network->sent.size(), 1);
    QTest::qWait(50);   // the 429 has arrived
    client()->get(QStringLiteral("b"), {}, {}, this, [&](const ApiReply &) { ++answered; });
    QTest::qWait(300);
    QCOMPARE(m_network->sent.size(), 1);
    QTRY_COMPARE_WITH_TIMEOUT(answered, 2, 3000);
    QCOMPARE(m_network->url(1), QStringLiteral("https://git.example/a"));
    QCOMPARE(m_network->url(2), QStringLiteral("https://git.example/b"));
}

void TestServerClient::givesUpAfterThree429()
{
    m_network->handler = [](const FakeNetwork::Sent &) { return Answer{429, {}, {{"Retry-After", "0"}}}; };
    const ApiReply reply = getAndWait(QStringLiteral("r"));
    QCOMPARE(reply.error, ApiReply::Error::Http);
    QCOMPARE(reply.httpStatus, 429);
    QCOMPARE(m_network->sent.size(), 4);
}

void TestServerClient::rateLimited403WaitsForReset()
{
    int calls = 0;
    m_network->handler = [&calls](const FakeNetwork::Sent &) {
        // A reset that has passed waits the shortest time, one second. The
        // header names are matched without regard to case.
        const QByteArray reset = QByteArray::number(QDateTime::currentSecsSinceEpoch() - 10);
        return ++calls == 1 ? Answer{403, {}, {{"X-RateLimit-Remaining", "0"}, {"X-RateLimit-Reset", reset}}}
                            : Answer{200, "{}", {}};
    };
    QSignalSpy changed(m_clients.get(), &ServerClients::failingChanged);
    QElapsedTimer timer;
    timer.start();
    const ApiReply reply = getAndWait(QStringLiteral("r"));
    QVERIFY(reply.ok());
    QVERIFY(timer.elapsed() >= 900);
    QCOMPARE(m_network->sent.size(), 2);
    QVERIFY(!m_clients->failing(kProfile, kModule));
    QVERIFY(changed.isEmpty());
}

void TestServerClient::rateLimited403WaitsForRetryAfter()
{
    int calls = 0;
    m_network->handler = [&calls](const FakeNetwork::Sent &) {
        return ++calls == 1 ? Answer{403, {}, {{"retry-after", "0"}}} : Answer{200, "{}", {}};
    };
    const ApiReply reply = getAndWait(QStringLiteral("r"));
    QVERIFY(reply.ok());
    QCOMPARE(m_network->sent.size(), 2);
    QVERIFY(!m_clients->failing(kProfile, kModule));
}

void TestServerClient::givesUpAfterThreeRateLimited403()
{
    m_network->handler = [](const FakeNetwork::Sent &) { return Answer{403, {}, {{"Retry-After", "0"}}}; };
    const ApiReply reply = getAndWait(QStringLiteral("r"));
    QCOMPARE(reply.error, ApiReply::Error::Http);
    QCOMPARE(reply.httpStatus, 403);
    QVERIFY(!reply.message.isEmpty());
    QCOMPARE(m_network->sent.size(), 4);
    QVERIFY(!m_clients->failing(kProfile, kModule));
}

void TestServerClient::refusedPausesUntilTokenChanges()
{
    m_network->handler = [](const FakeNetwork::Sent &sent) {
        return sent.request.rawHeader("X-Token") == "secret-2" ? Answer{200, "{}", {}} : Answer{401, {}, {}};
    };
    QSignalSpy changed(m_clients.get(), &ServerClients::failingChanged);

    ApiReply reply = getAndWait(QStringLiteral("api/v4/user"));
    QCOMPARE(reply.error, ApiReply::Error::Refused);
    QVERIFY(m_clients->failing(kProfile, kModule));
    QCOMPARE(m_clients->failingServers(), (QList<QPair<QString, QString>>{{kProfile, kModule}}));
    QCOMPARE(changed.size(), 1);
    QCOMPARE(changed.at(0).at(0).toString(), kProfile);
    QCOMPARE(changed.at(0).at(1).toString(), kModule);

    reply = getAndWait(QStringLiteral("api/v4/user"));
    QCOMPARE(reply.error, ApiReply::Error::Paused);
    QCOMPARE(m_network->sent.size(), 1);

    m_secrets->write(key(), QStringLiteral("secret-2"));
    QTRY_VERIFY(!m_clients->failing(kProfile, kModule));
    QCOMPARE(changed.size(), 2);
    reply = getAndWait(QStringLiteral("api/v4/user"));
    QVERIFY(reply.ok());
    QCOMPARE(header(m_network->sent.last(), "X-Token"), QByteArray("secret-2"));
}

void TestServerClient::settingsChangeResumes()
{
    m_network->handler = [](const FakeNetwork::Sent &) { return Answer{401, {}, {}}; };
    QCOMPARE(getAndWait(QStringLiteral("x")).error, ApiReply::Error::Refused);
    QVERIFY(m_clients->failing(kProfile, kModule));

    // A change to another setting of the workspace leaves it failing.
    m_model->setDefaultProfile(QStringLiteral("other"));
    QVERIFY(m_clients->failing(kProfile, kModule));

    m_directory->settings.screen[QStringLiteral("baseUrl")] = QStringLiteral("https://git2.example/");
    m_model->setDefaultProfile(kProfile);
    QVERIFY(!m_clients->failing(kProfile, kModule));
}

void TestServerClient::forbiddenGetPauses()
{
    // A 403 with requests left is no rate limit: it is not sent again.
    m_network->handler = [](const FakeNetwork::Sent &) {
        return Answer{403, {}, {{"x-ratelimit-remaining", "4999"}, {"x-ratelimit-reset", "1"}}};
    };
    QCOMPARE(getAndWait(QStringLiteral("x")).error, ApiReply::Error::Refused);
    QVERIFY(m_clients->failing(kProfile, kModule));
    QCOMPARE(m_network->sent.size(), 1);
}

void TestServerClient::forbiddenPostDoesNotPause()
{
    m_network->handler = [](const FakeNetwork::Sent &) { return Answer{403, {}, {}}; };
    QSignalSpy changed(m_clients.get(), &ServerClients::failingChanged);
    ApiReply result;
    bool answered = false;
    client()->post(QStringLiteral("approve"), {}, this, [&](const ApiReply &reply) {
        result = reply;
        answered = true;
    });
    QTRY_VERIFY(answered);
    QCOMPARE(result.error, ApiReply::Error::Forbidden);
    QVERIFY(!m_clients->failing(kProfile, kModule));
    QVERIFY(changed.isEmpty());
}

void TestServerClient::putSendsMethodAndBody()
{
    m_network->handler = [](const FakeNetwork::Sent &) { return Answer{200, R"({"id":2})", {}}; };
    ApiReply result;
    bool answered = false;
    client()->put(QStringLiteral("api/v4/things/2"), QJsonDocument(QJsonObject{{QStringLiteral("state_event"), QStringLiteral("close")}}),
                  this, [&](const ApiReply &reply) {
                      result = reply;
                      answered = true;
                  });
    QTRY_VERIFY(answered);
    QVERIFY(result.ok());
    QCOMPARE(result.json.object().value(QStringLiteral("id")).toInt(), 2);

    QCOMPARE(m_network->sent.size(), 1);
    const auto &sent = m_network->sent.at(0);
    QCOMPARE(sent.op, QNetworkAccessManager::PutOperation);
    QCOMPARE(m_network->url(0), QStringLiteral("https://git.example/api/v4/things/2"));
    QCOMPARE(sent.body, QByteArray(R"({"state_event":"close"})"));
    QCOMPARE(sent.request.header(QNetworkRequest::ContentTypeHeader).toByteArray(), QByteArray("application/json"));
    QCOMPARE(header(sent, "X-Token"), QByteArray("secret-1"));
}

void TestServerClient::putSendsNoEtag()
{
    m_network->handler = [](const FakeNetwork::Sent &) { return Answer{200, "{}", {{"ETag", "\"v1\""}}}; };
    QVERIFY(getAndWait(QStringLiteral("thing")).ok());

    bool answered = false;
    client()->put(QStringLiteral("thing"), {}, this, [&](const ApiReply &) { answered = true; });
    QTRY_VERIFY(answered);
    QCOMPARE(m_network->sent.size(), 2);
    QVERIFY(header(m_network->sent.at(1), "If-None-Match").isEmpty());

    // The PUT's answer is not kept, so the next GET still sends the old ETag.
    answered = false;
    client()->get(QStringLiteral("thing"), {}, {}, this, [&](const ApiReply &) { answered = true; });
    QTRY_VERIFY(answered);
    QCOMPARE(header(m_network->sent.at(2), "If-None-Match"), QByteArray("\"v1\""));
}

void TestServerClient::forbiddenPutDoesNotPause()
{
    m_network->handler = [](const FakeNetwork::Sent &) { return Answer{403, {}, {}}; };
    QSignalSpy changed(m_clients.get(), &ServerClients::failingChanged);
    ApiReply result;
    bool answered = false;
    client()->put(QStringLiteral("close"), {}, this, [&](const ApiReply &reply) {
        result = reply;
        answered = true;
    });
    QTRY_VERIFY(answered);
    QCOMPARE(result.error, ApiReply::Error::Forbidden);
    QVERIFY(!m_clients->failing(kProfile, kModule));
    QVERIFY(changed.isEmpty());
}

void TestServerClient::errorsMapped()
{
    m_network->handler = [](const FakeNetwork::Sent &sent) {
        const QString path = sent.request.url().path();
        if (path == QLatin1String("/missing"))
            return Answer{404, {}, {}};
        if (path == QLatin1String("/broken"))
            return Answer{500, {}, {}};
        return Answer{0, {}, {}};
    };
    ApiReply reply = getAndWait(QStringLiteral("missing"));
    QCOMPARE(reply.error, ApiReply::Error::NotFound);
    QCOMPARE(reply.httpStatus, 404);
    QVERIFY(!reply.message.isEmpty());
    reply = getAndWait(QStringLiteral("broken"));
    QCOMPARE(reply.error, ApiReply::Error::Http);
    QCOMPARE(reply.httpStatus, 500);
    reply = getAndWait(QStringLiteral("offline"));
    QCOMPARE(reply.error, ApiReply::Error::Network);
    QVERIFY(!reply.message.isEmpty());
    QVERIFY(!m_clients->failing(kProfile, kModule));
}

void TestServerClient::destroyedReceiverGetsNoCallback()
{
    m_network->handler = [](const FakeNetwork::Sent &) { return Answer{200, "[]", {}}; };
    bool called = false;
    auto *receiver = new QObject;
    client()->get(QStringLiteral("a"), {}, {}, receiver, [&](const ApiReply &) { called = true; });
    delete receiver;
    // A request without a receiver is always answered.
    bool other = false;
    client()->get(QStringLiteral("b"), {}, {}, nullptr, [&](const ApiReply &) { other = true; });
    QTRY_VERIFY(other);
    QTest::qWait(20);
    QVERIFY(!called);
}

void TestServerClient::queuedGetOfDestroyedReceiverIsNotSent()
{
    for (int i = 0; i < 4; ++i)
        client()->get(QStringLiteral("r%1").arg(i), {}, {}, this, [](const ApiReply &) {});
    QTRY_COMPARE(m_network->sent.size(), 4);

    // These wait in the queue. The receiver goes before their turn comes.
    auto *receiver = new QObject;
    client()->get(QStringLiteral("dropped"), {}, {}, receiver, [](const ApiReply &) {});
    client()->post(QStringLiteral("action"), QJsonDocument(QJsonObject()), receiver, [](const ApiReply &) {});
    client()->get(QStringLiteral("kept"), {}, {}, this, [](const ApiReply &) {});
    delete receiver;

    m_network->sent.at(0).reply->respond(200, "[]");
    m_network->sent.at(1).reply->respond(200, "[]");
    QTRY_COMPARE(m_network->sent.size(), 6);
    // The user asked for the POST, so it still goes.
    QCOMPARE(m_network->url(4), QStringLiteral("https://git.example/action"));
    QCOMPARE(m_network->url(5), QStringLiteral("https://git.example/kept"));
    for (int i = 2; i < m_network->sent.size(); ++i)
        m_network->sent.at(i).reply->respond(200, "[]");
    QTest::qWait(20);
    QCOMPARE(m_network->sent.size(), 6);
}

void TestServerClient::clearDropsRequestsInFlight()
{
    // One request stays in flight; another makes the client fail.
    bool called = false;
    client()->get(QStringLiteral("a"), {}, {}, this, [&](const ApiReply &) { called = true; });
    QTRY_COMPARE(m_network->sent.size(), 1);
    QPointer<FakeReply> inFlight = m_network->sent.at(0).reply;
    m_network->handler = [](const FakeNetwork::Sent &) { return Answer{401, {}, {}}; };
    QCOMPARE(getAndWait(QStringLiteral("b")).error, ApiReply::Error::Refused);

    QSignalSpy changed(m_clients.get(), &ServerClients::failingChanged);
    m_clients->clear();
    QCOMPARE(changed.size(), 1);
    QVERIFY(m_clients->failingServers().isEmpty());
    QVERIFY(!inFlight || inFlight->isFinished());
    QTest::qWait(20);
    QVERIFY(!called);
    // A new client starts without failing.
    QVERIFY(!m_clients->failing(kProfile, kModule));
    QVERIFY(client());
}

QTEST_MAIN(TestServerClient)
#include "tst_server_client.moc"
