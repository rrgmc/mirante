#include "api/SecretBackend.h"

// A stub of T0. Task T1 of the M5 plan implements it.

namespace mirante {

namespace {

class MemoryBackend : public SecretBackend
{
public:
    void read(const QString &key, std::function<void(std::optional<QString>, const QString &)> done) override
    {
        Q_UNUSED(key)
        done(std::nullopt, {});
    }

    void write(const QString &key, const QString &value, std::function<void(const QString &)> done) override
    {
        Q_UNUSED(key)
        Q_UNUSED(value)
        done({});
    }

    void remove(const QString &key, std::function<void(const QString &)> done) override
    {
        Q_UNUSED(key)
        done({});
    }
};

} // namespace

std::unique_ptr<SecretBackend> makeMemoryBackend()
{
    return std::make_unique<MemoryBackend>();
}

} // namespace mirante
