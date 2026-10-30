#include "api/SecretStore.h"

#include "api/ModuleDirectory.h"
#include "api/SecretBackend.h"
#include "core/Model.h"

#include <mirante/ModuleSettings.h>

#include <QHash>
#include <QPointer>

#include <vector>

// A token's value is never logged. Keys hold no secret.

namespace mirante {

QString secretKey(const QString &workspaceId, const QString &profileId, const QString &moduleId,
                  const QString &fieldKey)
{
    QStringList parts;
    if (!workspaceId.isEmpty())
        parts << workspaceId;
    parts << profileId << moduleId << fieldKey;
    return parts.join(QLatin1Char('/'));
}

QStringList tokenKeys(const Model &model, const ModuleDirectory &modules)
{
    QStringList keys;
    for (const Profile &profile : model.profiles()) {
        for (const QString &moduleId : modules.moduleIds()) {
            for (const SettingsScreen *screen : modules.settingsScreens(moduleId)) {
                for (const Field &field : screen->fields()) {
                    if (field.type != Field::Type::Secret)
                        continue;
                    const QString key = secretKey({}, profile.id, moduleId, field.key);
                    if (!keys.contains(key))
                        keys << key;
                }
            }
        }
    }
    return keys;
}

namespace {

using Answer = std::function<void(std::optional<QString>)>;

// A read() waiting for its answer. A receiver that was given and has since
// been destroyed drops the answer.
struct Waiter
{
    QPointer<QObject> receiver;
    bool hasReceiver = false;
    Answer done;

    void answer(const std::optional<QString> &value) const
    {
        if ((hasReceiver && !receiver) || !done)
            return;
        done(value);
    }
};

// Adds a workspace id to a key given without one.
QString withWorkspace(const QString &workspaceId, const QString &key)
{
    return workspaceId.isEmpty() ? key : workspaceId + QLatin1Char('/') + key;
}

} // namespace

struct SecretStore::Private
{
    SecretStore *q = nullptr;
    std::unique_ptr<SecretBackend> backend;
    // The keys whose state is known: the token, or nullopt if none is stored.
    QHash<QString, std::optional<QString>> cache;
    // The callbacks of keys being read from the backend.
    QHash<QString, std::vector<Answer>> loading;
    // The keys being moved by moveAll(), each with the number of moves using it.
    QHash<QString, int> moving;
    // The read() calls held back until their key's move is done.
    QHash<QString, std::vector<Waiter>> held;

    // Answers from the cache, or reads the key from the backend once, however
    // many callers ask while it loads. A keychain error answers nullopt and
    // leaves the key unknown, so a later read tries again.
    void load(const QString &key, Answer done)
    {
        const auto cachedIt = cache.constFind(key);
        if (cachedIt != cache.constEnd()) {
            done(*cachedIt);
            return;
        }
        auto &callbacks = loading[key];
        callbacks.push_back(std::move(done));
        if (callbacks.size() > 1)
            return;
        QPointer<SecretStore> alive = q;
        backend->read(key, [this, alive, key](std::optional<QString> value, const QString &error) {
            if (!alive)
                return;
            const std::vector<Answer> waiting = loading.take(key);
            const auto it = cache.constFind(key);
            if (it != cache.constEnd()) {
                // Written or removed while loading, which is newer.
                value = *it;
            } else if (!error.isEmpty()) {
                value.reset();
                emit q->failed(key, error);
            } else {
                cache.insert(key, value);
            }
            for (const Answer &callback : waiting)
                callback(value);
        });
    }

    // Writes the value, or removes the key for nullopt. The cache changes at
    // once. If the keychain refuses, the key becomes unknown again.
    void store(const QString &key, const std::optional<QString> &value, std::function<void()> finished = {})
    {
        cache.insert(key, value);
        emit q->changed(key);
        QPointer<SecretStore> alive = q;
        auto done = [this, alive, key, finished](const QString &error) {
            if (!alive)
                return;
            if (!error.isEmpty()) {
                cache.remove(key);
                emit q->failed(key, error);
                emit q->changed(key);
            }
            if (finished)
                finished();
        };
        if (value)
            backend->write(key, *value, std::move(done));
        else
            backend->remove(key, std::move(done));
    }

    // Moves one token, as moveAll() describes. The token is written under the
    // new key before the old one is removed, so a failure keeps it somewhere.
    void move(const QString &oldKey, const QString &newKey, std::function<void()> finished)
    {
        load(oldKey, [this, oldKey, newKey, finished](std::optional<QString> token) {
            if (!token) {
                finished();
                return;
            }
            load(newKey, [this, oldKey, newKey, token, finished](std::optional<QString> current) {
                auto removeOld = [this, oldKey, finished] { store(oldKey, std::nullopt, finished); };
                if (current)
                    removeOld();
                else
                    store(newKey, token, removeOld);
            });
        });
    }

    void hold(const QString &key) { ++moving[key]; }

    // Ends one move of the key. Reads held back for it go on once no move
    // uses it.
    void release(const QString &key)
    {
        auto it = moving.find(key);
        if (it == moving.end() || --*it > 0)
            return;
        moving.erase(it);
        const std::vector<Waiter> waiters = held.take(key);
        for (const Waiter &waiter : waiters)
            load(key, [waiter](std::optional<QString> value) { waiter.answer(value); });
    }
};

SecretStore::SecretStore(std::unique_ptr<SecretBackend> backend, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
    d->q = this;
    d->backend = std::move(backend);
}

SecretStore::~SecretStore() = default;

void SecretStore::read(const QString &key, QObject *receiver, std::function<void(std::optional<QString>)> done)
{
    Waiter waiter{receiver, receiver != nullptr, std::move(done)};
    if (d->moving.contains(key)) {
        d->held[key].push_back(std::move(waiter));
        return;
    }
    d->load(key, [waiter](std::optional<QString> value) { waiter.answer(value); });
}

std::optional<QString> SecretStore::cached(const QString &key) const
{
    return d->cache.value(key);
}

SecretStore::State SecretStore::state(const QString &key) const
{
    const auto it = d->cache.constFind(key);
    if (it == d->cache.constEnd())
        return State::Unknown;
    return it->has_value() ? State::Set : State::NotSet;
}

void SecretStore::write(const QString &key, const QString &value)
{
    d->store(key, value);
}

void SecretStore::remove(const QString &key)
{
    d->store(key, std::nullopt);
}

void SecretStore::removeAll(const QStringList &keys)
{
    for (const QString &key : keys)
        d->store(key, std::nullopt);
}

void SecretStore::moveAll(const QString &fromWorkspaceId, const QString &toWorkspaceId,
                          const QStringList &keysWithoutWorkspace, std::function<void()> done)
{
    QStringList keys = keysWithoutWorkspace;
    keys.removeDuplicates();
    if (fromWorkspaceId == toWorkspaceId || keys.isEmpty()) {
        if (done)
            done();
        return;
    }
    auto remaining = std::make_shared<int>(int(keys.size()));
    for (const QString &key : keys) {
        const QString oldKey = withWorkspace(fromWorkspaceId, key);
        const QString newKey = withWorkspace(toWorkspaceId, key);
        d->hold(oldKey);
        d->hold(newKey);
        d->move(oldKey, newKey, [this, oldKey, newKey, remaining, done] {
            d->release(oldKey);
            d->release(newKey);
            if (--*remaining == 0 && done)
                done();
        });
    }
}

} // namespace mirante
