// Tests SecretStore with the memory backend: reads load once, the cache and
// the state follow reads, writes and removes, changed and failed are emitted,
// removeAll removes only its keys, and moveAll moves tokens to a new
// workspace id as decision 0063 says. Also tests tokenKeys() and secretKey().
// It never touches the real keychain.
#include "api/ModuleDirectory.h"
#include "api/SecretBackend.h"
#include "api/SecretStore.h"
#include "core/Entities.h"
#include "core/Model.h"

#include <mirante/ModuleSettings.h>

#include <QHash>
#include <QObject>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>

#include <map>
#include <memory>

using namespace mirante;

namespace {

// Wraps the memory backend to count its reads, look at its values, and fail
// on demand, as a locked keychain would.
class TestBackend : public SecretBackend
{
public:
    TestBackend()
        : inner(makeMemoryBackend())
    {
    }

    void read(const QString &key, std::function<void(std::optional<QString>, const QString &)> done) override
    {
        ++reads[key];
        if (!error.isEmpty()) {
            QTimer::singleShot(0, [done = std::move(done), message = error] { done(std::nullopt, message); });
            return;
        }
        inner->read(key, std::move(done));
    }

    void write(const QString &key, const QString &value, std::function<void(const QString &)> done) override
    {
        if (!error.isEmpty()) {
            QTimer::singleShot(0, [done = std::move(done), message = error] { done(message); });
            return;
        }
        values[key] = value;
        inner->write(key, value, std::move(done));
    }

    void remove(const QString &key, std::function<void(const QString &)> done) override
    {
        if (!error.isEmpty()) {
            QTimer::singleShot(0, [done = std::move(done), message = error] { done(message); });
            return;
        }
        values.erase(key);
        inner->remove(key, std::move(done));
    }

    // Puts a value in without the store knowing, as an earlier run would.
    void seed(const QString &key, const QString &value)
    {
        values[key] = value;
        inner->write(key, value, [](const QString &) {});
    }

    std::unique_ptr<SecretBackend> inner;
    std::map<QString, QString> values;
    QHash<QString, int> reads;
    QString error;   // when set, every call fails with it
};

class TestScreen : public SettingsScreen
{
public:
    TestScreen(QString id, std::vector<Field> fields)
        : m_id(std::move(id))
        , m_fields(std::move(fields))
    {
    }

    QString id() const override { return m_id; }
    QString name() const override { return m_id; }
    std::vector<Field> fields() const override { return m_fields; }

private:
    QString m_id;
    std::vector<Field> m_fields;
};

class TestDirectory : public ModuleDirectory
{
public:
    QStringList moduleIds() const override
    {
        QStringList ids;
        for (const auto &[id, screens] : modules)
            ids << id;
        return ids;
    }
    const ItemType *findType(const QString &) const override { return nullptr; }
    const ServerType *serverType(const QString &) const override { return nullptr; }
    const ModuleSettings *moduleSettings(const QString &) const override { return nullptr; }
    std::vector<const SettingsScreen *> settingsScreens(const QString &moduleId) const override
    {
        std::vector<const SettingsScreen *> result;
        const auto it = modules.find(moduleId);
        if (it != modules.end()) {
            for (const auto &screen : it->second)
                result.push_back(screen.get());
        }
        return result;
    }

    std::map<QString, std::vector<std::unique_ptr<TestScreen>>> modules;
};

Field field(const QString &key, Field::Type type)
{
    Field result;
    result.key = key;
    result.type = type;
    return result;
}

Profile makeProfile(const QString &id)
{
    Profile profile;
    profile.id = id;
    profile.name = id;
    return profile;
}

} // namespace

class TestSecretStore : public QObject
{
    Q_OBJECT

private:
    TestBackend *backend = nullptr;
    std::unique_ptr<SecretStore> store;

    // Reads the key and waits for the answer.
    std::optional<QString> readNow(const QString &key)
    {
        bool answered = false;
        std::optional<QString> result;
        store->read(key, nullptr, [&](std::optional<QString> value) {
            answered = true;
            result = value;
        });
        if (!QTest::qWaitFor([&] { return answered; }))
            qFatal("read did not answer");
        return result;
    }

    // Lets the backend's queued answers arrive.
    static void settle()
    {
        for (int i = 0; i < 10; ++i)
            QCoreApplication::processEvents();
    }

private slots:
    void init()
    {
        auto owned = std::make_unique<TestBackend>();
        backend = owned.get();
        store = std::make_unique<SecretStore>(std::move(owned));
    }

    void cleanup()
    {
        store.reset();
        backend = nullptr;
    }

    void secretKeyJoinsTheParts()
    {
        QCOMPARE(secretKey(QStringLiteral("w"), QStringLiteral("default"), QStringLiteral("gitlab"),
                           QStringLiteral("token")),
                 QStringLiteral("w/default/gitlab/token"));
        QCOMPARE(secretKey({}, QStringLiteral("default"), QStringLiteral("gitlab"), QStringLiteral("token")),
                 QStringLiteral("default/gitlab/token"));
    }

    void readLoadsOnce()
    {
        backend->seed(QStringLiteral("w/p/m/token"), QStringLiteral("secret"));
        QCOMPARE(store->state(QStringLiteral("w/p/m/token")), SecretStore::State::Unknown);
        QVERIFY(!store->cached(QStringLiteral("w/p/m/token")));

        // Two reads while loading ask the backend once.
        int answers = 0;
        for (int i = 0; i < 2; ++i) {
            store->read(QStringLiteral("w/p/m/token"), nullptr, [&](std::optional<QString> value) {
                QCOMPARE(value, std::optional<QString>(QStringLiteral("secret")));
                ++answers;
            });
        }
        QCOMPARE(answers, 0);
        QTRY_COMPARE(answers, 2);
        QCOMPARE(backend->reads.value(QStringLiteral("w/p/m/token")), 1);

        // Once loaded, a read answers at once.
        store->read(QStringLiteral("w/p/m/token"), nullptr, [&](std::optional<QString>) { ++answers; });
        QCOMPARE(answers, 3);
        QCOMPARE(backend->reads.value(QStringLiteral("w/p/m/token")), 1);
        QCOMPARE(store->cached(QStringLiteral("w/p/m/token")), std::optional<QString>(QStringLiteral("secret")));
        QCOMPARE(store->state(QStringLiteral("w/p/m/token")), SecretStore::State::Set);
    }

    void readOfAMissingKeyIsNotSet()
    {
        QVERIFY(!readNow(QStringLiteral("w/p/m/token")));
        QCOMPARE(store->state(QStringLiteral("w/p/m/token")), SecretStore::State::NotSet);
        QVERIFY(!readNow(QStringLiteral("w/p/m/token")));
        QCOMPARE(backend->reads.value(QStringLiteral("w/p/m/token")), 1);
    }

    void destroyedReceiverGetsNoAnswer()
    {
        auto receiver = std::make_unique<QObject>();
        bool answered = false;
        store->read(QStringLiteral("k"), receiver.get(), [&](std::optional<QString>) { answered = true; });
        receiver.reset();
        settle();
        QVERIFY(!answered);
        QCOMPARE(store->state(QStringLiteral("k")), SecretStore::State::NotSet);
    }

    void writeAndRemoveUpdateTheCache()
    {
        QSignalSpy changed(store.get(), &SecretStore::changed);

        store->write(QStringLiteral("k"), QStringLiteral("one"));
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed.at(0).at(0).toString(), QStringLiteral("k"));
        QCOMPARE(store->state(QStringLiteral("k")), SecretStore::State::Set);
        QCOMPARE(store->cached(QStringLiteral("k")), std::optional<QString>(QStringLiteral("one")));
        settle();
        QCOMPARE(backend->values.at(QStringLiteral("k")), QStringLiteral("one"));
        QCOMPARE(readNow(QStringLiteral("k")), std::optional<QString>(QStringLiteral("one")));
        QCOMPARE(backend->reads.value(QStringLiteral("k")), 0);

        store->remove(QStringLiteral("k"));
        QCOMPARE(changed.count(), 2);
        QCOMPARE(store->state(QStringLiteral("k")), SecretStore::State::NotSet);
        QVERIFY(!store->cached(QStringLiteral("k")));
        settle();
        QCOMPARE(backend->values.count(QStringLiteral("k")), size_t(0));
    }

    void writeWhileLoadingWins()
    {
        backend->seed(QStringLiteral("k"), QStringLiteral("old"));
        std::optional<QString> answer;
        bool answered = false;
        store->read(QStringLiteral("k"), nullptr, [&](std::optional<QString> value) {
            answered = true;
            answer = value;
        });
        store->write(QStringLiteral("k"), QStringLiteral("new"));
        QTRY_VERIFY(answered);
        QCOMPARE(answer, std::optional<QString>(QStringLiteral("new")));
        QCOMPARE(store->cached(QStringLiteral("k")), std::optional<QString>(QStringLiteral("new")));
    }

    void keychainErrorsEmitFailed()
    {
        QSignalSpy failed(store.get(), &SecretStore::failed);
        backend->error = QStringLiteral("locked");

        QVERIFY(!readNow(QStringLiteral("k")));
        QCOMPARE(failed.count(), 1);
        QCOMPARE(failed.at(0).at(0).toString(), QStringLiteral("k"));
        QCOMPARE(failed.at(0).at(1).toString(), QStringLiteral("locked"));
        // A failed read is not remembered, so a later read tries again.
        QCOMPARE(store->state(QStringLiteral("k")), SecretStore::State::Unknown);

        QSignalSpy changed(store.get(), &SecretStore::changed);
        store->write(QStringLiteral("k"), QStringLiteral("value"));
        QTRY_COMPARE(failed.count(), 2);
        QCOMPARE(store->state(QStringLiteral("k")), SecretStore::State::Unknown);
        QCOMPARE(changed.count(), 2);

        backend->error.clear();
        QVERIFY(!readNow(QStringLiteral("k")));
        QCOMPARE(store->state(QStringLiteral("k")), SecretStore::State::NotSet);
    }

    void removeAllRemovesOnlyItsKeys()
    {
        backend->seed(QStringLiteral("a"), QStringLiteral("1"));
        backend->seed(QStringLiteral("b"), QStringLiteral("2"));
        backend->seed(QStringLiteral("c"), QStringLiteral("3"));
        QSignalSpy changed(store.get(), &SecretStore::changed);

        store->removeAll({QStringLiteral("a"), QStringLiteral("c")});
        settle();
        QCOMPARE(changed.count(), 2);
        QCOMPARE(backend->values.size(), size_t(1));
        QCOMPARE(backend->values.at(QStringLiteral("b")), QStringLiteral("2"));
        QCOMPARE(store->state(QStringLiteral("a")), SecretStore::State::NotSet);
        QCOMPARE(store->state(QStringLiteral("b")), SecretStore::State::Unknown);
    }

    void moveAllMovesTokens()
    {
        // "p/gitlab/token" moves. "p/jira/token" stays, since the new id
        // already has one. "q/gitlab/token" is not set anywhere.
        backend->seed(QStringLiteral("old/p/gitlab/token"), QStringLiteral("g"));
        backend->seed(QStringLiteral("old/p/jira/token"), QStringLiteral("j-old"));
        backend->seed(QStringLiteral("new/p/jira/token"), QStringLiteral("j-new"));
        backend->seed(QStringLiteral("other/p/gitlab/token"), QStringLiteral("x"));

        int doneCalls = 0;
        store->moveAll(QStringLiteral("old"), QStringLiteral("new"),
                       {QStringLiteral("p/gitlab/token"), QStringLiteral("p/jira/token"),
                        QStringLiteral("q/gitlab/token")},
                       [&] { ++doneCalls; });
        QCOMPARE(doneCalls, 0);
        QTRY_COMPARE(doneCalls, 1);
        settle();
        QCOMPARE(doneCalls, 1);

        const std::map<QString, QString> expected{
            {QStringLiteral("new/p/gitlab/token"), QStringLiteral("g")},
            {QStringLiteral("new/p/jira/token"), QStringLiteral("j-new")},
            {QStringLiteral("other/p/gitlab/token"), QStringLiteral("x")},
        };
        QCOMPARE(backend->values, expected);
        QCOMPARE(store->state(QStringLiteral("old/p/gitlab/token")), SecretStore::State::NotSet);
        QCOMPARE(store->cached(QStringLiteral("new/p/gitlab/token")), std::optional<QString>(QStringLiteral("g")));
        // A key not set is skipped without looking under the new id.
        QCOMPARE(backend->reads.value(QStringLiteral("new/q/gitlab/token")), 0);
    }

    void moveAllHoldsBackReads()
    {
        backend->seed(QStringLiteral("old/p/gitlab/token"), QStringLiteral("g"));
        bool moved = false;
        std::optional<QString> answer;
        bool answered = false;

        store->moveAll(QStringLiteral("old"), QStringLiteral("new"), {QStringLiteral("p/gitlab/token")},
                       [&] { moved = true; });
        // Without the hold, this read would find nothing under the new key.
        store->read(QStringLiteral("new/p/gitlab/token"), nullptr, [&](std::optional<QString> value) {
            QVERIFY(!moved);
            answered = true;
            answer = value;
        });
        QTRY_VERIFY(moved);
        QVERIFY(answered);
        QCOMPARE(answer, std::optional<QString>(QStringLiteral("g")));
    }

    void moveAllWithNothingToDoCallsDone()
    {
        int doneCalls = 0;
        store->moveAll(QStringLiteral("a"), QStringLiteral("a"), {QStringLiteral("p/m/k")}, [&] { ++doneCalls; });
        store->moveAll(QStringLiteral("a"), QStringLiteral("b"), {}, [&] { ++doneCalls; });
        QCOMPARE(doneCalls, 2);
    }

    void tokenKeysListsSecretFieldsOfEveryProfile()
    {
        TestDirectory modules;
        modules.modules[QStringLiteral("gitlab")].push_back(std::make_unique<TestScreen>(
            QStringLiteral("general"), std::vector<Field>{field(QStringLiteral("baseUrl"), Field::Type::Url),
                                                          field(QStringLiteral("token"), Field::Type::Secret)}));
        modules.modules[QStringLiteral("jira")].push_back(std::make_unique<TestScreen>(
            QStringLiteral("general"), std::vector<Field>{field(QStringLiteral("token"), Field::Type::Secret)}));
        modules.modules[QStringLiteral("jira")].push_back(std::make_unique<TestScreen>(
            QStringLiteral("extra"), std::vector<Field>{field(QStringLiteral("password"), Field::Type::Secret),
                                                        field(QStringLiteral("token"), Field::Type::Secret)}));
        modules.modules[QStringLiteral("notes")];

        Model model;
        model.setProfiles({makeProfile(QStringLiteral("default")), makeProfile(QStringLiteral("work"))});

        const QStringList expected{
            QStringLiteral("default/gitlab/token"), QStringLiteral("default/jira/token"),
            QStringLiteral("default/jira/password"), QStringLiteral("work/gitlab/token"),
            QStringLiteral("work/jira/token"), QStringLiteral("work/jira/password"),
        };
        QCOMPARE(tokenKeys(model, modules), expected);
    }
};

QTEST_GUILESS_MAIN(TestSecretStore)
#include "tst_secret_store.moc"
