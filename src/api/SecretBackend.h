#pragma once

#include <QString>

#include <functional>
#include <memory>
#include <optional>

namespace mirante {

// Where SecretStore keeps tokens. Every call is asynchronous: done is called
// later, on the main thread, exactly once. See the M5 plan, part 2.
class SecretBackend
{
public:
    virtual ~SecretBackend() = default;

    // done gets the value, or nullopt if none is stored. error is empty on
    // success.
    virtual void read(const QString &key,
                      std::function<void(std::optional<QString> value, const QString &error)> done) = 0;
    virtual void write(const QString &key, const QString &value, std::function<void(const QString &error)> done) = 0;
    // Removing a key that is not stored succeeds.
    virtual void remove(const QString &key, std::function<void(const QString &error)> done) = 0;
};

// The operating system's keychain, through qtkeychain, under the service
// "mirante". Implemented in KeychainBackend.cpp.
std::unique_ptr<SecretBackend> makeKeychainBackend();
// The name of a key's entry in the keychain: "mirante/<key>". See decision
// 0064.
QString keychainEntryName(const QString &key);

// Values in memory only, lost at exit. For tests and --no-keychain.
// Implemented in MemoryBackend.cpp.
std::unique_ptr<SecretBackend> makeMemoryBackend();

} // namespace mirante
