#include "web/SingleInstance.h"

#include <QCryptographicHash>
#include <QDeadlineTimer>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QThread>

namespace mirante {

namespace {

// The handover is one line of JSON from the later start, such as
// {"data":"C:/work"}, answered with "ok". Longer messages are refused.
constexpr qsizetype kMaxMessage = 64 * 1024;
const QByteArray kAnswer = QByteArrayLiteral("ok\n");
const QString kDataKey = QStringLiteral("data");

// The milliseconds left, at least 1, for the socket's wait functions.
int remaining(const QDeadlineTimer &deadline)
{
    return int(qMax<qint64>(1, deadline.remainingTime()));
}

QString normalisedFolder(const QString &folder)
{
    const QString absolute = QDir::cleanPath(QFileInfo(folder).absoluteFilePath());
    QString path = QFileInfo(absolute).canonicalFilePath();
    if (path.isEmpty())
        path = absolute;
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    // The usual file systems there ignore case.
    path = path.toLower();
#endif
    return path;
}

} // namespace

SingleInstance::SingleInstance(const QString &webDataFolder, QObject *parent)
    : QObject(parent)
    , m_folder(webDataFolder)
    , m_lock(QDir(webDataFolder).filePath(QStringLiteral(".lock")))
{
    QDir().mkpath(m_folder);
    // The lock is held as long as the app runs, so its age says nothing. A
    // lock is still taken over when the process that wrote it no longer runs.
    m_lock.setStaleLockTime(0);
}

SingleInstance::~SingleInstance() = default;

bool SingleInstance::tryLock()
{
    if (m_lock.isLocked())
        return true;
    if (!m_lock.tryLock(0))
        return false;

    m_server = std::make_unique<QLocalServer>();
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    connect(m_server.get(), &QLocalServer::newConnection, this, &SingleInstance::acceptConnection);
    const QString name = serverName(m_folder);
    // A crashed holder can leave its socket file behind on macOS and Linux.
    // Holding the lock means no other process listens on this name.
    QLocalServer::removeServer(name);
    if (!m_server->listen(name))
        qWarning("Later starts cannot hand over: %ls", qUtf16Printable(m_server->errorString()));
    return true;
}

bool SingleInstance::isLocked() const
{
    return m_lock.isLocked();
}

bool SingleInstance::heldElsewhere() const
{
    return !m_lock.isLocked() && m_lock.error() == QLockFile::LockFailedError;
}

qint64 SingleInstance::holderPid() const
{
    qint64 pid = 0;
    QString host;
    QString app;
    if (!m_lock.getLockInfo(&pid, &host, &app))
        return 0;
    return pid;
}

bool SingleInstance::handOver(const QString &dataFolder, int timeoutMs) const
{
    const QDeadlineTimer deadline(timeoutMs);
    const QString name = serverName(m_folder);
    QLocalSocket socket;
    for (;;) {
        socket.connectToServer(name);
        if (socket.waitForConnected(remaining(deadline)))
            break;
        socket.abort();
        if (deadline.hasExpired())
            return false;
        QThread::msleep(100);
    }

    QJsonObject message;
    if (!dataFolder.isEmpty())
        message.insert(kDataKey, dataFolder);
    socket.write(QJsonDocument(message).toJson(QJsonDocument::Compact) + '\n');
    // On Windows the write may be done already, and then there is nothing to
    // wait for.
    if (socket.bytesToWrite() > 0 && !socket.waitForBytesWritten(remaining(deadline)))
        return false;

    // The holder answers and closes the connection. Both may arrive in one
    // wait, so what was read is checked before a failed wait counts.
    QByteArray answer;
    for (;;) {
        answer += socket.readAll();
        if (answer.contains('\n'))
            break;
        if (deadline.hasExpired())
            return false;
        if (!socket.waitForReadyRead(remaining(deadline))) {
            answer += socket.readAll();
            if (answer.contains('\n'))
                break;
            if (socket.state() != QLocalSocket::ConnectedState)
                return false;
        }
    }
    socket.disconnectFromServer();
    return answer == kAnswer;
}

QString SingleInstance::serverName(const QString &webDataFolder)
{
    const QByteArray key = (normalisedFolder(webDataFolder) + QLatin1Char('\n') + QDir::homePath()).toUtf8();
    const QByteArray hash = QCryptographicHash::hash(key, QCryptographicHash::Sha256).left(16).toHex();
    return QStringLiteral("mirante-") + QString::fromLatin1(hash);
}

void SingleInstance::acceptConnection()
{
    while (QLocalSocket *socket = m_server->nextPendingConnection()) {
        connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QLocalSocket::readyRead, this, [this, socket] {
            if (!socket->canReadLine()) {
                if (socket->bytesAvailable() > kMaxMessage)
                    socket->abort();
                return;
            }
            const QByteArray line = socket->readLine(kMaxMessage);
            const QJsonObject message = QJsonDocument::fromJson(line).object();
            socket->write(kAnswer);
            socket->flush();
            socket->disconnectFromServer();
            emit activationRequested(message.value(kDataKey).toString());
        });
    }
}

} // namespace mirante
