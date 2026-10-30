#include "api/SecretBackend.h"

#include <QHash>
#include <QTimer>

// Values in a map. Each call changes the map at once and calls done from the
// event loop, so callers see the same order of events as with the keychain.

namespace mirante {

namespace {

class MemoryBackend : public SecretBackend
{
public:
    void read(const QString &key, std::function<void(std::optional<QString>, const QString &)> done) override
    {
        const auto it = values.constFind(key);
        std::optional<QString> value;
        if (it != values.constEnd())
            value = *it;
        QTimer::singleShot(0, [value, done = std::move(done)] { done(value, {}); });
    }

    void write(const QString &key, const QString &value, std::function<void(const QString &)> done) override
    {
        values.insert(key, value);
        QTimer::singleShot(0, [done = std::move(done)] { done({}); });
    }

    void remove(const QString &key, std::function<void(const QString &)> done) override
    {
        values.remove(key);
        QTimer::singleShot(0, [done = std::move(done)] { done({}); });
    }

private:
    QHash<QString, QString> values;
};

} // namespace

std::unique_ptr<SecretBackend> makeMemoryBackend()
{
    return std::make_unique<MemoryBackend>();
}

} // namespace mirante
