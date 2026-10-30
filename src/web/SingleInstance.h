#pragma once

#include <QLockFile>
#include <QObject>
#include <QString>

#include <memory>

class QLocalServer;

namespace mirante {

// Keeps one Mirante process per web data folder, the folder that holds the
// profiles' logins. Qt WebEngine does not stop two processes from using the
// same profile folders, and two would damage each other's cookie and site
// databases. See docs/decisions/0058-one-instance-per-web-data-folder.md.
//
// The process that takes the lock, `.lock` in the folder, also listens on a
// local server named after the folder. A later start that finds the lock held
// hands over to it: it sends the workspace given with --data, if any, and
// exits. The holder then emits activationRequested().
class SingleInstance : public QObject
{
    Q_OBJECT

public:
    // The folder is made if it is missing.
    explicit SingleInstance(const QString &webDataFolder, QObject *parent = nullptr);
    ~SingleInstance() override;

    // Takes the lock and starts listening for later starts. A lock left by a
    // process that no longer runs is taken over. False when another process
    // holds the lock, or the folder cannot be written.
    bool tryLock();
    bool isLocked() const;
    // True when tryLock() failed because another process holds the lock, not
    // because the folder cannot be written.
    bool heldElsewhere() const;

    // The process id of the lock's holder, after tryLock() failed. 0 when it
    // is not known.
    qint64 holderPid() const;

    // Sends the workspace folder, or an empty string for none, to the process
    // holding the lock, and waits for its answer. Retries the connection until
    // the timeout, since the holder may have taken the lock a moment ago and
    // not be listening yet. True when the holder confirmed. Blocks; it needs
    // no event loop.
    bool handOver(const QString &dataFolder, int timeoutMs = 5000) const;

    // The local server's name for a web data folder: "mirante-" and a hash of
    // the folder's absolute path and the user's home folder, so it is a valid
    // name on every platform and differs per user.
    static QString serverName(const QString &webDataFolder);

signals:
    // A later start handed over. dataFolder is the absolute path given with
    // its --data, or empty.
    void activationRequested(const QString &dataFolder);

private:
    void acceptConnection();

    QString m_folder;
    QLockFile m_lock;
    std::unique_ptr<QLocalServer> m_server;
};

} // namespace mirante
