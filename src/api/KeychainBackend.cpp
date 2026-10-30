#include "api/SecretBackend.h"

#include <qtkeychain/keychain.h>

#include <QObject>

// The operating system's keychain through qtkeychain: Credential Manager on
// Windows, Keychain on macOS and Secret Service on Linux. Each call starts a
// job that deletes itself when it finishes. See docs/sync.md#secrets.
//
// Each entry is named "mirante/<key>". Credential Manager on Windows ignores
// the service and lists entries by key alone, so the prefix tells whose they
// are (decision 0064).

namespace mirante {

namespace {

const QString kService = QStringLiteral("mirante");

class KeychainBackend : public SecretBackend
{
public:
    void read(const QString &key, std::function<void(std::optional<QString>, const QString &)> done) override
    {
        auto *job = new QKeychain::ReadPasswordJob(kService);
        job->setAutoDelete(true);
        job->setKey(keychainEntryName(key));
        QObject::connect(job, &QKeychain::Job::finished, job, [job, done = std::move(done)] {
            switch (job->error()) {
            case QKeychain::NoError:
                done(job->textData(), {});
                break;
            case QKeychain::EntryNotFound:
                done(std::nullopt, {});
                break;
            default:
                done(std::nullopt, job->errorString());
                break;
            }
        });
        job->start();
    }

    void write(const QString &key, const QString &value, std::function<void(const QString &)> done) override
    {
        auto *job = new QKeychain::WritePasswordJob(kService);
        job->setAutoDelete(true);
        job->setKey(keychainEntryName(key));
        job->setTextData(value);
        QObject::connect(job, &QKeychain::Job::finished, job, [job, done = std::move(done)] {
            done(job->error() == QKeychain::NoError ? QString() : job->errorString());
        });
        job->start();
    }

    void remove(const QString &key, std::function<void(const QString &)> done) override
    {
        auto *job = new QKeychain::DeletePasswordJob(kService);
        job->setAutoDelete(true);
        job->setKey(keychainEntryName(key));
        QObject::connect(job, &QKeychain::Job::finished, job, [job, done = std::move(done)] {
            const bool ok = job->error() == QKeychain::NoError || job->error() == QKeychain::EntryNotFound;
            done(ok ? QString() : job->errorString());
        });
        job->start();
    }
};

} // namespace

QString keychainEntryName(const QString &key)
{
    return kService + QLatin1Char('/') + key;
}

std::unique_ptr<SecretBackend> makeKeychainBackend()
{
    return std::make_unique<KeychainBackend>();
}

} // namespace mirante
