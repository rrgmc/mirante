#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>
#include <optional>

namespace mirante {

class Model;
class ModuleDirectory;
class SecretBackend;

// The keychain key of a token: "<workspace id>/<profile id>/<module
// id>/<field key>", such as "5a3e…/default/gitlab/token". Without a workspace
// id, the first part is left out. See docs/sync.md#secrets and decision 0059.
QString secretKey(const QString &workspaceId, const QString &profileId, const QString &moduleId,
                  const QString &fieldKey);

// "<profile id>/<module id>/<field key>" for every profile of the model and
// every Secret field of every module's settings screens: the keys a workspace
// can have, without its id. The keychain cannot list its keys, so removing
// and moving tokens works from this list. See decision 0063.
QStringList tokenKeys(const Model &model, const ModuleDirectory &modules);

// Tokens on this device. Values read once are kept in memory, so later reads
// answer at once. See the M5 plan, part 2. Implemented in SecretStore.cpp.
class SecretStore : public QObject
{
    Q_OBJECT

public:
    explicit SecretStore(std::unique_ptr<SecretBackend> backend, QObject *parent = nullptr);
    ~SecretStore() override;

    // Loads the token if needed. done is dropped if receiver is destroyed
    // first. A key being moved by moveAll() answers after its move.
    void read(const QString &key, QObject *receiver, std::function<void(std::optional<QString>)> done);
    // The token if already loaded, without asking the keychain.
    std::optional<QString> cached(const QString &key) const;

    enum class State { Unknown, Set, NotSet };
    // Unknown until the key was read, written or removed.
    State state(const QString &key) const;

    void write(const QString &key, const QString &value);
    void remove(const QString &key);
    // Removes the given full keys.
    void removeAll(const QStringList &keys);

    // Moves each key, given without its workspace id, from one workspace id to
    // another: reads the token under the old key, writes it under the new one
    // unless one is there already, then removes the old one. Keys not set are
    // skipped. done is called once, after all keys. See decision 0063.
    void moveAll(const QString &fromWorkspaceId, const QString &toWorkspaceId,
                 const QStringList &keysWithoutWorkspace, std::function<void()> done = {});

signals:
    // A key was written, removed or moved.
    void changed(const QString &key);
    // The keychain refused a call. The key's token then counts as not set.
    void failed(const QString &key, const QString &message);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
