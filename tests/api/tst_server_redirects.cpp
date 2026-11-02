// Tests that a token follows a redirect only on its server's origin. These
// tests use a real QNetworkAccessManager against two local HTTP servers,
// because the question is what Qt does with the token header on a redirect.
// See docs/decisions/0123-tokens-stay-on-their-origin.md.
#include "api/ModuleDirectory.h"
#include "api/SecretBackend.h"
#include "api/SecretStore.h"
#include "api/ServerClient.h"
#include "core/Model.h"

#include <mirante/ApiClient.h>
#include <mirante/ModuleSettings.h>

#include <QHash>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

#include <memory>

using namespace mirante;

namespace {

const QString kWorkspace = QStringLiteral("ws");
const QString kProfile = QStringLiteral("default");
const QString kModule = QStringLiteral("srv");

// Answers each request with the next reply, and keeps the requests.
class HttpServer : public QObject
{
public:
    struct Reply
    {
        int status = 200;
        QByteArray location;
        QByteArray body;
    };
    QList<Reply> replies;
    QList<QByteArray> requests;

    HttpServer()
    {
        m_server.listen(QHostAddress::LocalHost);
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket *socket = m_server.nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, socket, [this, socket] { read(socket); });
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
    }

    QByteArray url(const QByteArray &path = {}) const
    {
        return "http://127.0.0.1:" + QByteArray::number(m_server.serverPort()) + "/" + path;
    }

private:
    void read(QTcpSocket *socket)
    {
        QByteArray &buffer = m_buffers[socket];
        buffer += socket->readAll();
        if (!buffer.contains("\r\n\r\n"))
            return;
        requests << buffer;
        buffer.clear();
        const Reply reply = replies.isEmpty() ? Reply{404, {}, "missing"} : replies.takeFirst();
        QByteArray response = "HTTP/1.1 " + QByteArray::number(reply.status) + " X\r\n";
        if (!reply.location.isEmpty())
            response += "Location: " + reply.location + "\r\n";
        response += "Content-Length: " + QByteArray::number(reply.body.size()) + "\r\n";
        response += "Connection: close\r\n\r\n" + reply.body;
        socket->write(response);
        socket->disconnectFromHost();
    }

    QTcpServer m_server;
    QHash<QTcpSocket *, QByteArray> m_buffers;
};

class TestServerType : public ServerType
{
public:
    QString screenId() const override { return QStringLiteral("general"); }
    QString tokenKey() const override { return QStringLiteral("token"); }
    QUrl baseUrl(const QJsonObject &values) const override { return QUrl(values.value(QStringLiteral("baseUrl")).toString()); }
    void authorize(QNetworkRequest &request, const QString &token, const QJsonObject &) const override
    {
        request.setRawHeader("X-Token", token.toUtf8());
    }
};

class TestSettings : public ModuleSettings
{
public:
    QJsonObject screen;
    QJsonObject values(const QString &, const QString &) const override { return screen; }
};

class TestDirectory : public ModuleDirectory
{
public:
    TestServerType type;
    TestSettings settings;

    QStringList moduleIds() const override { return {kModule}; }
    const ItemType *findType(const QString &) const override { return nullptr; }
    const ServerType *serverType(const QString &) const override { return &type; }
    const ModuleSettings *moduleSettings(const QString &) const override { return &settings; }
    std::vector<const SettingsScreen *> settingsScreens(const QString &) const override { return {}; }
};

} // namespace

class TestServerRedirects : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void followsOnTheSameOrigin();
    void relativeLocationFollows();
    void refusesAnotherOrigin();
    void refusesAnotherOriginOfAPost();
    void refusesAnotherOriginAfterASameOriginStep();

private:
    ApiClient *client() { return m_clients->client(kProfile, kModule); }

    std::unique_ptr<Model> m_model;
    std::unique_ptr<TestDirectory> m_directory;
    std::unique_ptr<SecretStore> m_secrets;
    std::unique_ptr<QNetworkAccessManager> m_network;
    std::unique_ptr<ServerClients> m_clients;
    std::unique_ptr<HttpServer> m_server;
    std::unique_ptr<HttpServer> m_other;
};

void TestServerRedirects::init()
{
    m_server = std::make_unique<HttpServer>();
    m_other = std::make_unique<HttpServer>();
    m_model = std::make_unique<Model>();
    m_model->setWorkspaceId(kWorkspace);
    m_directory = std::make_unique<TestDirectory>();
    m_directory->settings.screen = {{QStringLiteral("baseUrl"), QString::fromLatin1(m_server->url())}};
    m_secrets = std::make_unique<SecretStore>(makeMemoryBackend());
    m_network = std::make_unique<QNetworkAccessManager>();
    m_clients = std::make_unique<ServerClients>(*m_model, *m_directory, *m_secrets, m_network.get());

    const QString key = secretKey(kWorkspace, kProfile, kModule, QStringLiteral("token"));
    m_secrets->write(key, QStringLiteral("secret-1"));
    QTRY_COMPARE(m_secrets->state(key), SecretStore::State::Set);
}

void TestServerRedirects::cleanup()
{
    m_clients.reset();
    m_network.reset();
    m_secrets.reset();
    m_directory.reset();
    m_model.reset();
    m_other.reset();
    m_server.reset();
}

void TestServerRedirects::followsOnTheSameOrigin()
{
    m_server->replies = {{302, m_server->url("new"), {}}, {200, {}, R"({"a":1})"}};

    ApiReply result;
    bool answered = false;
    client()->get(QStringLiteral("old"), {}, {}, this, [&](const ApiReply &reply) {
        result = reply;
        answered = true;
    });
    QTRY_VERIFY(answered);

    QVERIFY2(result.ok(), qPrintable(result.message));
    QCOMPARE(result.json.object().value(QStringLiteral("a")).toInt(), 1);
    QCOMPARE(m_server->requests.size(), 2);
    QVERIFY(m_server->requests.at(1).contains("GET /new "));
    QVERIFY(m_server->requests.at(1).toLower().contains("x-token: secret-1"));
}

void TestServerRedirects::relativeLocationFollows()
{
    m_server->replies = {{301, "/new", {}}, {200, {}, "{}"}};

    ApiReply result;
    bool answered = false;
    client()->get(QStringLiteral("old"), {}, {}, this, [&](const ApiReply &reply) {
        result = reply;
        answered = true;
    });
    QTRY_VERIFY(answered);

    QVERIFY2(result.ok(), qPrintable(result.message));
    QCOMPARE(m_server->requests.size(), 2);
}

void TestServerRedirects::refusesAnotherOrigin()
{
    m_server->replies = {{302, m_other->url("stolen"), {}}};
    m_other->replies = {{200, {}, "{}"}};

    ApiReply result;
    bool answered = false;
    client()->get(QStringLiteral("old"), {}, {}, this, [&](const ApiReply &reply) {
        result = reply;
        answered = true;
    });
    QTRY_VERIFY(answered);
    // A request that was on its way would have arrived by now.
    QTest::qWait(100);

    QCOMPARE(m_other->requests.size(), 0);
    QCOMPARE(result.error, ApiReply::Error::OtherServer);
    QVERIFY(result.message.contains(QString::fromLatin1(m_other->url("stolen"))));
}

void TestServerRedirects::refusesAnotherOriginOfAPost()
{
    m_server->replies = {{307, m_other->url("stolen"), {}}};
    m_other->replies = {{200, {}, "{}"}};

    ApiReply result;
    bool answered = false;
    client()->post(QStringLiteral("old"), QJsonDocument(QJsonObject{{QStringLiteral("a"), 1}}), this,
                   [&](const ApiReply &reply) {
                       result = reply;
                       answered = true;
                   });
    QTRY_VERIFY(answered);
    QTest::qWait(100);

    QCOMPARE(m_other->requests.size(), 0);
    QCOMPARE(result.error, ApiReply::Error::OtherServer);
}

void TestServerRedirects::refusesAnotherOriginAfterASameOriginStep()
{
    m_server->replies = {{302, m_server->url("step"), {}}, {302, m_other->url("stolen"), {}}};
    m_other->replies = {{200, {}, "{}"}};

    ApiReply result;
    bool answered = false;
    client()->get(QStringLiteral("old"), {}, {}, this, [&](const ApiReply &reply) {
        result = reply;
        answered = true;
    });
    QTRY_VERIFY(answered);
    QTest::qWait(100);

    QCOMPARE(m_server->requests.size(), 2);
    QCOMPARE(m_other->requests.size(), 0);
    QCOMPARE(result.error, ApiReply::Error::OtherServer);
}

QTEST_MAIN(TestServerRedirects)
#include "tst_server_redirects.moc"
